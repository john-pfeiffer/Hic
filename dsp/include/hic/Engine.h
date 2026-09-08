#pragma once
#include <cstdint>
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Kit.h"
#include "hic/Events.h"
#include "hic/Humanizer.h"
#include "hic/VoiceAllocator.h"
#include "hic/Bed.h"
#include "hic/seq/Clock.h"
#include "hic/fx/Ducker.h"
#include "hic/fx/SpringReverb.h"
#include "hic/fx/BeatRepeat.h"
#include "hic/fx/GranularFreeze.h"

namespace hic {

struct GlobalParams {
    float outputDb      = 0.0f;
    bool  internalPlay  = false;    // internal transport when the host gives none
    double internalBpm  = 96.0;
    int   beatsPerBar   = 4;
};

/// The whole instrument. The plugin (or a hardware main loop) owns one of
/// these, writes the public parameter structs between blocks, and calls
/// process() with the block's note events.
class Engine {
public:
    GlobalParams  global;
    KitParams     kit;
    FeelParams    feel;
    BedParams     bed;
    DuckParams    duck;
    ReverbParams  reverb;
    RepeatParams  repeat;
    FreezeParams  freeze;

    void prepare(float sampleRate);

    /// `in` holds this block's events with sampleTime relative to the block start.
    void process(const NoteEvent* in, int nIn, const TransportInfo& transport, float* outL, float* outR, int n);

    /// Convenience for tests and the GUI audition: hit a pad at the start of the next block.
    bool queueHit(int pad, float vel, int note = -1, int offset = 0);

    int     padForNote(int note) const { return kit.noteToPad[note & 127]; }
    float   sampleRate() const { return sr_; }
    int     activeVoices() const { return voices_.activeCount(); }
    int     activeVoices(int pad) const { return voices_.activeCount(pad); }
    int64_t position() const { return blockStart_; }
    int     lookaheadSamples() const { return lookahead_; }
    const Clock& clock() const { return clock_; }
    const BeatRepeat& beatRepeat() const { return repeat_; }
    bool    freezeHeld() const { return freeze_.held(); }

    /// Hooks used by the sequencer (Phase 3) and tests.
    void    setBedGate(bool open) { bedGateExternal_ = open; }

private:
    void processChunk(const TransportInfo& transport, float* outL, float* outR, int n);
    void renderVoices(int from, int to);
    void fire(const NoteEvent& e);
    void updateBedGate(int n);

    float sr_ = 48000.0f;
    int64_t blockStart_ = 0;
    int lookahead_ = 0;
    uint32_t hitCounter_ = 0;
    bool bedGateExternal_ = true;
    EventQueue queue_;
    NoteEvent due_[kMaxEvents];
    NoteEvent pending_[kMaxEvents];
    int nPending_ = 0;
    VoiceAllocator voices_;
    Clock clock_;
    Bed bed_;
    Ducker ducker_;
    SpringReverb reverb_;
    BeatRepeat repeat_;
    GranularFreeze freeze_;

    float dryL_[kMaxBlock] = {};
    float dryR_[kMaxBlock] = {};
    float revSend_[kMaxBlock] = {};
    float freezeTap_[kMaxBlock] = {};
    float duckGain_[kMaxBlock] = {};
    float scratch_[kMaxBlock] = {};
};

} // namespace hic
