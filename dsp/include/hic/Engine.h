#pragma once
#include <cstdint>
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Kit.h"
#include "hic/Key.h"
#include "hic/Bus.h"
#include "hic/Events.h"
#include "hic/Humanizer.h"
#include "hic/VoiceAllocator.h"
#include "hic/StaticTexture.h"
#include "hic/seq/Clock.h"
#include "hic/seq/Pattern.h"
#include "hic/seq/Sequencer.h"
#include "hic/fx/Ducker.h"
#include "hic/fx/SpringReverb.h"
#include "hic/fx/BeatRepeat.h"
#include "hic/fx/GranularFreeze.h"
#include "hic/fx/BusShaper.h"
#include "hic/fx/Damp.h"

namespace hic {

struct GlobalParams {
    float  outputDb      = 0.0f;
    bool   internalPlay  = false;   // internal transport when the host gives none
    double internalBpm   = 96.0;
    int    beatsPerBar   = 4;
    bool   seqEnabled    = true;    // internal sequencer runs whenever the clock plays
    int    activePattern = 0;       // 0..kNumPatterns-1
};

/// The whole instrument. The plugin (or a hardware main loop) owns one of
/// these, writes the public parameter structs between blocks, and calls
/// process() with the block's note events.
///
/// Rule for the six bus macros: the macro scales, the detail struct shapes.
///  bus.drive   -> BusShaper
///  bus.damp    -> Damp
///  bus.texture -> grit inside every hit and statik.levelDetail
///  bus.space   -> reverb.mix and a size scale on reverb.decaySec
///  bus.drift   -> multiplier on every pad's Drift macro
///  bus.feel    -> multiplier on feel.scatterMs and feel.velScatter (their maxima)
class Engine {
public:
    GlobalParams  global;
    KitParams     kit;
    BusParams     bus;
    KeyParams     key;
    FeelParams    feel;        // detail: maxima for scatter, plus nudge, lookahead and seed
    StaticParams  statik;      // detail
    DuckParams    duck;
    ReverbParams  reverb;      // detail: type, decay, damp, predelay (mix comes from bus.space)
    RepeatParams  repeat;
    FreezeParams  freeze;
    Pattern       patterns[kNumPatterns];
    int           patternVersion = -1;   // host bookkeeping: which published pattern set is loaded

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
    const VoiceAllocator& voices() const { return voices_; }
    const Pattern& activePattern() const { return patterns[clamp(global.activePattern, 0, kNumPatterns - 1)]; }
    /// Step a track is on right now (for playheads); -1 when stopped.
    int currentStep(int track) const { return clock_.playing() ? Sequencer::stepAt(activePattern(), track, clock_.ppqStart()) : -1; }

    /// The feel actually applied this block (detail scaled by bus.feel).
    FeelParams effectiveFeel() const;

private:
    void processChunk(const TransportInfo& transport, float* outL, float* outR, int n);
    void enqueue(NoteEvent e, int n);
    void renderVoices(int from, int to);
    void fire(const NoteEvent& e);
    void updateStaticPulse(int n);

    float sr_ = 48000.0f;
    int64_t blockStart_ = 0;
    int lookahead_ = 0;
    uint32_t hitCounter_ = 0;
    EventQueue queue_;
    NoteEvent due_[kMaxEvents];
    NoteEvent pending_[kMaxEvents];
    int nPending_ = 0;
    VoiceAllocator voices_;
    Clock clock_;
    Sequencer seq_;
    NoteEvent seqEvents_[kMaxEvents];
    StaticTexture static_;
    Ducker ducker_;
    SpringReverb reverb_;
    BeatRepeat repeat_;
    GranularFreeze freeze_;
    BusShaper shaper_;
    Damp damp_;

    float dryL_[kMaxBlock] = {};
    float dryR_[kMaxBlock] = {};
    float revSend_[kMaxBlock] = {};
    float freezeTap_[kMaxBlock] = {};
    float duckGain_[kMaxBlock] = {};
    int64_t pulse_[kMaxBlock] = {};
    uint32_t pulseSeed_[kMaxBlock] = {};
    float scratch_[kMaxBlock] = {};
};

} // namespace hic
