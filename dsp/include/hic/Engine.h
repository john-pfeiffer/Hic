#pragma once
#include <cstdint>
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Kit.h"
#include "hic/Events.h"
#include "hic/VoiceAllocator.h"

namespace hic {

/// Timing and dynamics feel. Applies to host MIDI and the sequencer alike.
struct FeelParams {
    float    nudgeMs     = 0.0f;   // constant offset, -20..20
    float    scatterMs   = 3.0f;   // random per-hit timing scatter (max)
    float    velScatter  = 0.08f;  // random per-hit velocity scatter (0..1)
    float    lookaheadMs = 0.0f;   // delay applied to every event so hits can move early
    uint32_t seed        = 1;
};

/// The whole instrument. The plugin (or a hardware main loop) owns one of
/// these, writes the public parameter structs between blocks, and calls
/// process() with the block's note events.
class Engine {
public:
    KitParams  kit;
    FeelParams feel;

    void prepare(float sampleRate);

    /// `in` holds this block's events with sampleTime relative to the block start.
    void process(const NoteEvent* in, int nIn, const TransportInfo& transport, float* outL, float* outR, int n);

    /// Convenience for tests and the GUI audition: hit a pad at the start of the next block.
    bool queueHit(int pad, float vel, int note = -1, int offset = 0);

    int   padForNote(int note) const { return kit.noteToPad[note & 127]; }
    float sampleRate() const { return sr_; }
    int   activeVoices() const { return voices_.activeCount(); }
    int   activeVoices(int pad) const { return voices_.activeCount(pad); }
    int64_t position() const { return blockStart_; }
    int   lookaheadSamples() const { return lookahead_; }

private:
    void processChunk(const TransportInfo& transport, float* outL, float* outR, int n);
    void renderVoices(int from, int to);
    void fire(const NoteEvent& e);

    float sr_ = 48000.0f;
    int64_t blockStart_ = 0;
    int lookahead_ = 0;
    uint32_t hitCounter_ = 0;
    EventQueue queue_;
    NoteEvent due_[kMaxEvents];
    NoteEvent pending_[kMaxEvents];
    int nPending_ = 0;
    VoiceAllocator voices_;

    float dryL_[kMaxBlock] = {};
    float dryR_[kMaxBlock] = {};
    float revSend_[kMaxBlock] = {};
    float freezeTap_[kMaxBlock] = {};
    float scratch_[kMaxBlock] = {};
};

} // namespace hic
