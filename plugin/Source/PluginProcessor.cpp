#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "hic/Kit.h"

using namespace juce;

static const char kPatternMagic[8] = { 'H', 'I', 'C', 'P', 'A', 'T', '0', '1' };

HicProcessor::HicProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HicState2", hicplug::createLayout()) {   // v2: v1 state is ignored, patterns still load
    hic::makeDefaultKit(defaultKit_);
    engine = std::make_unique<hic::Engine>();
    engine->kit = defaultKit_;
    bridgeImpl = std::make_unique<hicplug::EngineBridge>(apvts);
    for (auto& s : stepNow) s.store(-1);
}

HicProcessor::~HicProcessor() = default;

bool HicProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == AudioChannelSet::mono();
}

void HicProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    bridgeImpl->apply(*engine);
    engine->prepare(static_cast<float>(sampleRate));
    lastLatency = engine->lookaheadSamples();
    setLatencySamples(lastLatency);
}

void HicProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;

    bridgeImpl->apply(*engine);
    if (engine->lookaheadSamples() != lastLatency) { lastLatency = engine->lookaheadSamples(); setLatencySamples(lastLatency); }

    // MIDI in: note-ons play pads; note-offs matter only for the freeze hold note.
    int nEv = 0;
    for (const auto meta : midi) {
        const MidiMessage m = meta.getMessage();
        if (nEv >= hic::kMaxEvents) break;
        if (m.isNoteOn()) {
            hic::NoteEvent e;
            e.sampleTime = meta.samplePosition;
            e.note = static_cast<uint8_t>(m.getNoteNumber());
            e.vel = static_cast<uint8_t>(jmax(1, static_cast<int>(m.getVelocity())));
            e.source = static_cast<uint8_t>(hic::EventSource::Midi);
            events[nEv++] = e;
        } else if (m.isNoteOff() && m.getNoteNumber() == hic::kNoteFreezeHold) {
            hic::NoteEvent e;
            e.sampleTime = meta.samplePosition;
            e.note = static_cast<uint8_t>(m.getNoteNumber());
            e.source = static_cast<uint8_t>(hic::EventSource::Midi);
            e.flags = hic::EvNoteOff;
            events[nEv++] = e;
        }
    }
    midi.clear();

    const int pad = auditionPad_.exchange(-1);
    if (pad >= 0) engine->queueHit(pad, auditionVel_.load(), pad == hic::PadGlock ? 72 : (pad == hic::PadThumb ? 45 : 36));

    hic::TransportInfo t;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            t.valid = pos->getPpqPosition().hasValue() && pos->getBpm().hasValue();
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto sig = pos->getTimeSignature()) { t.num = sig->numerator; t.den = sig->denominator; }
        }
    }
    // In "Internal" sync the host transport is ignored entirely.
    if (engine->global.internalPlay || *apvts.getRawParameterValue(hicplug::id::sync) > 0.5f) t.valid = false;

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    if (R == nullptr) {
        // Mono host: render stereo into L and a scratch, then sum.
        static thread_local std::vector<float> scratch;
        if (static_cast<int>(scratch.size()) < n) scratch.resize(static_cast<size_t>(n));
        engine->process(events, nEv, t, L, scratch.data(), n);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + scratch[static_cast<size_t>(i)]);
    } else {
        engine->process(events, nEv, t, L, R, n);
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

    playing.store(engine->clock().playing());
    for (int k = 0; k < hic::kNumPads; ++k) stepNow[k].store(engine->currentStep(k));
}

AudioProcessorEditor* HicProcessor::createEditor() { return new HicEditor(*this); }

void HicProcessor::getStateInformation(MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    MemoryBlock blob;
    blob.append(kPatternMagic, sizeof(kPatternMagic));
    const uint32 size = static_cast<uint32>(sizeof(hic::Pattern) * hic::kNumPatterns);
    blob.append(&size, sizeof(size));
    blob.append(bridgeImpl->allEditPatterns(), size);
    xml->setAttribute("patterns", blob.toBase64Encoding());
    copyXmlToBinary(*xml, destData);
}

void HicProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    const String b64 = xml->getStringAttribute("patterns");
    xml->removeAttribute("patterns");
    apvts.replaceState(ValueTree::fromXml(*xml));
    MemoryBlock blob;
    if (b64.isNotEmpty() && blob.fromBase64Encoding(b64)) {
        const size_t header = sizeof(kPatternMagic) + sizeof(uint32);
        if (blob.getSize() >= header && std::memcmp(blob.getData(), kPatternMagic, sizeof(kPatternMagic)) == 0) {
            uint32 size = 0;
            std::memcpy(&size, static_cast<const char*>(blob.getData()) + sizeof(kPatternMagic), sizeof(size));
            if (size == sizeof(hic::Pattern) * hic::kNumPatterns && blob.getSize() >= header + size)
                bridgeImpl->loadPatterns(static_cast<const char*>(blob.getData()) + header, size);
        }
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HicProcessor(); }
