#include "hic/Engine.h"

namespace hic {

void Engine::prepare(float sampleRate) {
    sr_ = sampleRate < kMinSampleRate ? kMinSampleRate : sampleRate;
    voices_.prepare(sr_);
    clock_.prepare(sr_);
    bed_.prepare(sr_);
    ducker_.prepare(sr_);
    reverb_.prepare(sr_);
    repeat_.prepare(sr_);
    freeze_.prepare(sr_);
    queue_.clear();
    blockStart_ = 0;
    nPending_ = 0;
    hitCounter_ = 0;
    lookahead_ = static_cast<int>(feel.lookaheadMs * 0.001f * sr_);
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

    auto queueEvent = [&](NoteEvent e) {
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
            Humanizer::apply(e, feel, kit.pads[e.pad < kNumPads ? e.pad : 0], sr_);
        }
        queue_.push(e);
    };
    for (int i = 0; i < nIn; ++i) queueEvent(in[i]);
    for (int i = 0; i < nPending_; ++i) queueEvent(pending_[i]);
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

void Engine::updateBedGate(int n) {
    // Rhythmic gate from the clock; the sequencer may override via setBedGate.
    bool open = bedGateExternal_;
    if (bed.gate != BedGate::Off && bed.gate != BedGate::Steps) {
        if (!clock_.playing()) open = true;
        else {
            const double period = bed.gate == BedGate::Sixteenths ? 0.25 : (bed.gate == BedGate::Eighths ? 0.5 : 1.0);
            const double ppq = clock_.ppqAt(n / 2);
            const double phase = ppq / period - std::floor(ppq / period);
            open = phase < static_cast<double>(clamp(bed.gateDuty, 0.05f, 0.95f));
        }
    }
    bed_.setGate(open);
}

void Engine::processChunk(const TransportInfo& transport, float* outL, float* outR, int n) {
    clock_.setInternalPlaying(global.internalPlay);
    clock_.setInternalBpm(global.internalBpm);
    clock_.setBeatsPerBar(global.beatsPerBar);
    clock_.update(transport, n);

    for (int i = 0; i < n; ++i) { dryL_[i] = 0.0f; dryR_[i] = 0.0f; revSend_[i] = 0.0f; freezeTap_[i] = 0.0f; }

    // Voices, split at event boundaries so hits are sample-accurate.
    const int nDue = queue_.popDue(blockStart_ + n, due_, kMaxEvents);
    int cursor = 0;
    for (int i = 0; i < nDue; ++i) {
        int at = static_cast<int>(due_[i].sampleTime - blockStart_);
        if (at < cursor) at = cursor;   // late events (moved early past the block start) fire now
        if (at > cursor) { renderVoices(cursor, at); ducker_.render(duckGain_ + cursor, at - cursor); cursor = at; }
        fire(due_[i]);
    }
    if (cursor < n) { renderVoices(cursor, n); ducker_.render(duckGain_ + cursor, n - cursor); }

    // Bed, ducked by the kick, gated by the clock.
    updateBedGate(n);
    ducker_.set(duck);
    bed_.render(dryL_, dryR_, duckGain_, n, bed);

    // Short reverb from the per-pad sends.
    reverb_.set(reverb);
    reverb_.process(revSend_, dryL_, dryR_, n);

    // Rare stutter, then the freeze texture on top.
    repeat_.process(dryL_, dryR_, n, repeat, clock_, feel.seed);
    freeze_.process(freezeTap_, dryL_, dryR_, n, freeze);

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
    if (e.flags & EvBedGate) { bedGateExternal_ = true; }
    const float vel = static_cast<float>(e.vel) * (1.0f / 127.0f);
    const uint32_t seed = e.seed != 0 ? e.seed : hashSeed(feel.seed, e.note, ++hitCounter_);
    if (e.pad < kNumPads && (kit.pads[e.pad].flags & PadDuckSource)) ducker_.trigger();
    voices_.noteOn(kit, e.pad, e.note, vel, seed, (e.flags & EvReverse) != 0);
}

} // namespace hic
