#include "hic/Engine.h"

namespace hic {

void Engine::prepare(float sampleRate) {
    sr_ = sampleRate < kMinSampleRate ? kMinSampleRate : sampleRate;
    voices_.prepare(sr_);
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

    // Absolute-time every incoming event and queue it.
    for (int i = 0; i < nIn; ++i) {
        NoteEvent e = in[i];
        if (e.source == static_cast<uint8_t>(EventSource::Midi)) e.pad = kit.noteToPad[e.note & 127];
        e.sampleTime = blockStart_ + clamp<int64_t>(e.sampleTime, 0, n - 1) + lookahead_;
        queue_.push(e);
    }
    for (int i = 0; i < nPending_; ++i) {
        NoteEvent e = pending_[i];
        e.sampleTime = blockStart_ + clamp<int64_t>(e.sampleTime, 0, n - 1) + lookahead_;
        queue_.push(e);
    }
    nPending_ = 0;

    int done = 0;
    while (done < n) {
        const int chunk = (n - done) < kMaxBlock ? (n - done) : kMaxBlock;
        processChunk(transport, outL + done, outR + done, chunk);
        done += chunk;
    }
}

void Engine::processChunk(const TransportInfo& /*transport*/, float* outL, float* outR, int n) {
    for (int i = 0; i < n; ++i) { dryL_[i] = 0.0f; dryR_[i] = 0.0f; revSend_[i] = 0.0f; freezeTap_[i] = 0.0f; }

    const int nDue = queue_.popDue(blockStart_ + n, due_, kMaxEvents);
    int cursor = 0;
    for (int i = 0; i < nDue; ++i) {
        int at = static_cast<int>(due_[i].sampleTime - blockStart_);
        if (at < cursor) at = cursor;   // late events (queued in the past) fire immediately
        if (at > cursor) { renderVoices(cursor, at); cursor = at; }
        fire(due_[i]);
    }
    if (cursor < n) renderVoices(cursor, n);

    for (int i = 0; i < n; ++i) {
        outL[i] = softClip(dryL_[i]);
        outR[i] = softClip(dryR_[i]);
    }
    blockStart_ += n;
}

void Engine::renderVoices(int from, int to) {
    voices_.render(dryL_ + from, dryR_ + from, revSend_ + from, freezeTap_ + from, scratch_, to - from);
}

void Engine::fire(const NoteEvent& e) {
    if (e.flags & EvNoPad) return;
    const float vel = static_cast<float>(e.vel) * (1.0f / 127.0f);
    const uint32_t seed = hashSeed(feel.seed ^ (static_cast<uint32_t>(kit.seed) << 8), e.note, ++hitCounter_);
    voices_.noteOn(kit, e.pad, e.note, vel, seed, (e.flags & EvReverse) != 0);
}

} // namespace hic
