#include "hic/Engine.h"
#include <cstdint>
#include <cmath>

namespace hic {

void Engine::prepare(float sampleRate) {
    sr_ = sampleRate < kMinSampleRate ? kMinSampleRate : sampleRate;
    voices_.prepare(sr_);
    clock_.prepare(sr_);
    seq_.prepare(sr_);
    static_.prepare(sr_, hashSeed(feel.seed, 0x5741u));
    ducker_.prepare(sr_);
    reverb_.prepare(sr_);
    repeat_.prepare(sr_);
    freeze_.prepare(sr_);
    damp_.prepare(sr_);
    queue_.clear();
    blockStart_ = 0;
    nPending_ = 0;
    hitCounter_ = 0;
    lookahead_ = static_cast<int>(feel.lookaheadMs * 0.001f * sr_);
}

FeelParams Engine::effectiveFeel() const {
    FeelParams f = feel;
    const float amount = clamp(bus.feel, 0.0f, 1.0f);
    f.scatterMs = feel.scatterMs * amount;
    f.velScatter = feel.velScatter * amount;
    return f;
}

bool Engine::queueHit(int pad, float vel, int note, int offset) {
    if (nPending_ >= kMaxEvents) return false;
    NoteEvent e;
    e.sampleTime = offset;
    e.pad = static_cast<uint8_t>(pad);
    e.note = static_cast<uint8_t>(note >= 0 ? note : 36);
    e.vel = static_cast<uint8_t>(clamp(vel, 0.0f, 1.0f) * 127.0f + 0.5f);
    e.source = static_cast<uint8_t>(EventSource::Internal);
    pending_[nPending_++] = e;
    return true;
}

void Engine::process(const NoteEvent* in, int nIn, const TransportInfo& transport, float* outL, float* outR, int n) {
    lookahead_ = static_cast<int>(feel.lookaheadMs * 0.001f * sr_);

    for (int i = 0; i < nIn; ++i) enqueue(in[i], n);
    for (int i = 0; i < nPending_; ++i) enqueue(pending_[i], n);
    nPending_ = 0;

    int done = 0;
    while (done < n) {
        const int chunk = (n - done) < kMaxBlock ? (n - done) : kMaxBlock;
        TransportInfo t = transport;
        if (t.valid) t.ppq += t.bpm / 60.0 * static_cast<double>(done) / static_cast<double>(sr_);
        processChunk(t, outL + done, outR + done, chunk);
        done += chunk;
    }
}

void Engine::enqueue(NoteEvent e, int n) {
    if (e.source == static_cast<uint8_t>(EventSource::Midi)) {
        if (e.note == kNoteFreezeHold || e.note == kNoteForceRepeat) e.flags |= EvNoPad;
        e.pad = kit.noteToPad[e.note & 127];
    }
    e.sampleTime = blockStart_ + clamp<int64_t>(e.sampleTime, 0, n - 1) + lookahead_;
    if (!(e.flags & (EvNoPad | EvNoteOff))) {
        if (e.seed == 0)
            e.seed = (e.source == static_cast<uint8_t>(EventSource::Seq))
                       ? hashSeed(feel.seed, e.bar, static_cast<uint32_t>(e.step) | (static_cast<uint32_t>(e.pad) << 16))
                       : hashSeed(feel.seed, e.note, ++hitCounter_);
        Humanizer::apply(e, effectiveFeel(), kit.pads[e.pad < kNumPads ? e.pad : 0], sr_);
    }
    queue_.push(e);
}

void Engine::updateStaticPulse(int n) {
    // The static only exists inside pulses: on a clock division, or on flagged
    // steps. Gating is per sample so a bar replays exactly.
    if (statik.clock == StaticClock::Off || !clock_.playing()) { for (int i = 0; i < n; ++i) { pulse_[i] = -1; pulseSeed_[i] = 0u; } return; }
    const double spb = static_cast<double>(activePattern().stepsPerBeat < 1 ? 1 : activePattern().stepsPerBeat);
    const int64_t beatsPerBar = clock_.beatsPerBar() < 1 ? 1 : clock_.beatsPerBar();
    // Pulses are seeded by their position in the bar, so every bar of static is the same bar.
    if (statik.clock == StaticClock::Steps) {
        const int64_t perBar = static_cast<int64_t>(spb) * beatsPerBar;
        int64_t lastStep = INT64_MIN; bool open = false;
        for (int i = 0; i < n; ++i) {
            const double ppq = clock_.ppqAt(i);
            const int64_t stepIdx = static_cast<int64_t>(std::floor(ppq * spb + 1e-9));
            if (stepIdx != lastStep) { lastStep = stepIdx; open = global.seqEnabled && Sequencer::bedGateAt(activePattern(), ppq); }
            pulse_[i] = open ? stepIdx : -1;
            pulseSeed_[i] = static_cast<uint32_t>(((stepIdx % perBar) + perBar) % perBar);
        }
        return;
    }
    const double period = statik.clock == StaticClock::Sixteenths ? 0.25 : (statik.clock == StaticClock::Eighths ? 0.5 : 1.0);
    const int64_t perBar = static_cast<int64_t>(static_cast<double>(beatsPerBar) / period + 0.5);
    // Integer sample positions, like the sequencer, so a pulse edge never wobbles by a sample.
    const double samplesPerBeat = clock_.samplesPerBeat();
    const int64_t blockStart = static_cast<int64_t>(std::llround(clock_.ppqStart() * samplesPerBeat));
    const int64_t pulseSamples = static_cast<int64_t>(clamp(statik.pulseMs, 1.0f, 2000.0f) * 0.001f * sr_);
    const double periodSamples = period * samplesPerBeat;
    for (int i = 0; i < n; ++i) {
        const int64_t s = blockStart + i;
        const int64_t idx = static_cast<int64_t>(std::floor(static_cast<double>(s) / periodSamples + 1e-9));
        const int64_t start = static_cast<int64_t>(std::llround(static_cast<double>(idx) * periodSamples));
        pulse_[i] = (s - start) < pulseSamples ? idx : -1;
        pulseSeed_[i] = static_cast<uint32_t>(((idx % perBar) + perBar) % perBar);
    }
}

void Engine::processChunk(const TransportInfo& transport, float* outL, float* outR, int n) {
    clock_.setInternalPlaying(global.internalPlay);
    clock_.setInternalBpm(global.internalBpm);
    clock_.setBeatsPerBar(global.beatsPerBar);
    clock_.update(transport, n);

    // Internal sequencer feeds the same queue as host MIDI.
    if (global.seqEnabled && clock_.playing()) {
        const int nSeq = seq_.process(activePattern(), kit, clock_, n, feel.seed, seqEvents_, kMaxEvents);
        for (int i = 0; i < nSeq; ++i) enqueue(seqEvents_[i], n);
    }

    for (int i = 0; i < n; ++i) { dryL_[i] = 0.0f; dryR_[i] = 0.0f; revSend_[i] = 0.0f; freezeTap_[i] = 0.0f; }

    // Voices, split at event boundaries so hits are sample-accurate.
    const int nDue = queue_.popDue(blockStart_ + n, due_, kMaxEvents);
    int cursor = 0;
    for (int i = 0; i < nDue; ++i) {
        int at = static_cast<int>(due_[i].sampleTime - blockStart_);
        if (at < cursor) at = cursor;
        if (at > cursor) { renderVoices(cursor, at); ducker_.render(duckGain_ + cursor, at - cursor); cursor = at; }
        fire(due_[i]);
    }
    if (cursor < n) { renderVoices(cursor, n); ducker_.render(duckGain_ + cursor, n - cursor); }

    // Clocked static, ducked by the kick.
    updateStaticPulse(n);
    ducker_.set(duck);
    static_.render(dryL_, dryR_, duckGain_, pulse_, pulseSeed_, n, statik, clamp(bus.texture, 0.0f, 1.0f) * statik.levelDetail);

    // Bus drive, then the short reverb from the per-pad sends.
    shaper_.set(bus.drive);
    shaper_.process(dryL_, dryR_, n);
    {
        ReverbParams r = reverb;
        const float space = clamp(bus.space, 0.0f, 1.0f);
        r.mix = space;
        r.decaySec = reverb.decaySec * lerp(0.5f, 1.5f, space);
        reverb_.set(r);
        if (space > 0.0f || !reverb_.quiet()) reverb_.process(revSend_, dryL_, dryR_, n);
    }

    // Rare stutter, the freeze texture, then the blanket.
    repeat_.process(dryL_, dryR_, n, repeat, clock_, feel.seed);
    freeze_.process(freezeTap_, dryL_, dryR_, n, freeze);
    damp_.set(bus.damp);
    damp_.process(dryL_, dryR_, n);

    const float out = dbToGain(clamp(global.outputDb, -60.0f, 12.0f));
    for (int i = 0; i < n; ++i) {
        outL[i] = softClip(dryL_[i] * out);
        outR[i] = softClip(dryR_[i] * out);
    }
    blockStart_ += n;
}

void Engine::renderVoices(int from, int to) {
    voices_.render(dryL_ + from, dryR_ + from, revSend_ + from, freezeTap_ + from, scratch_, to - from);
}

void Engine::fire(const NoteEvent& e) {
    if (e.flags & EvNoPad) {
        if (e.note == kNoteFreezeHold) freeze_.setExternalHold(!(e.flags & EvNoteOff));
        else if (e.note == kNoteForceRepeat && !(e.flags & EvNoteOff)) repeat_.force();
        return;
    }
    if (e.flags & EvNoteOff) return;
    const float vel = static_cast<float>(e.vel) * (1.0f / 127.0f);
    const uint32_t seed = e.seed != 0 ? e.seed : hashSeed(feel.seed, e.note, ++hitCounter_);
    if (e.pad < kNumPads && (kit.pads[e.pad].flags & PadDuckSource)) ducker_.trigger();
    voices_.noteOn(kit, e.pad, e.note, vel, seed, (e.flags & EvReverse) != 0, key, clamp(bus.drift, 0.0f, 2.0f), clamp(bus.texture, 0.0f, 1.0f));
}

} // namespace hic
