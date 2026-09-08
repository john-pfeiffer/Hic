#pragma once
#include <cmath>
#include "hic/Events.h"

namespace hic {

/// Musical time for one block. Follows the host transport when it is valid
/// and playing; otherwise runs an internal clock (standalone, hardware).
class Clock {
public:
    void prepare(float sr) { sr_ = sr; ppqPerSample_ = bpm_ / 60.0 / static_cast<double>(sr); }

    // Internal transport (used when the host gives nothing).
    void setInternalPlaying(bool on) { if (on && !internalPlaying_) internalPpq_ = 0.0; internalPlaying_ = on; }
    void setInternalBpm(double bpm) { internalBpm_ = bpm < 20.0 ? 20.0 : (bpm > 300.0 ? 300.0 : bpm); }
    void setBeatsPerBar(int n) { beatsPerBar_ = n < 1 ? 1 : n; }
    bool internalPlaying() const { return internalPlaying_; }

    /// Call once per block before using the accessors. `n` is the block size.
    void update(const TransportInfo& t, int n) {
        const bool host = t.valid;
        const double prevEnd = ppqStart_ + ppqPerSample_ * lastN_;
        if (host) {
            playing_ = t.playing;
            bpm_ = t.bpm > 1.0 ? t.bpm : 120.0;
            ppqStart_ = t.ppq;
            beatsPerBar_ = t.den > 0 ? (t.num * 4) / t.den : 4;
        } else {
            playing_ = internalPlaying_;
            bpm_ = internalBpm_;
            ppqStart_ = internalPpq_;
        }
        ppqPerSample_ = playing_ ? bpm_ / 60.0 / static_cast<double>(sr_) : 0.0;
        discontinuity_ = wasPlaying_ != playing_ || std::fabs(ppqStart_ - prevEnd) > ppqPerSample_ * n + 1e-6;
        wasPlaying_ = playing_;
        lastN_ = n;
        if (!host && playing_) internalPpq_ += ppqPerSample_ * n;
    }

    bool   playing() const { return playing_; }
    double bpm() const { return bpm_; }
    int    beatsPerBar() const { return beatsPerBar_; }
    double ppqStart() const { return ppqStart_; }
    double ppqPerSample() const { return ppqPerSample_; }
    double ppqAt(int sampleOffset) const { return ppqStart_ + ppqPerSample_ * sampleOffset; }
    /// Sample offset (relative to block start) for a ppq; may be outside the block.
    double sampleFor(double ppq) const { return ppqPerSample_ > 0.0 ? (ppq - ppqStart_) / ppqPerSample_ : 0.0; }
    double samplesPerBeat() const { return 60.0 / bpm_ * static_cast<double>(sr_); }
    bool   discontinuity() const { return discontinuity_; }
    float  sampleRate() const { return sr_; }

private:
    float  sr_ = 48000.0f;
    double bpm_ = 120.0, internalBpm_ = 120.0;
    double ppqStart_ = 0.0, internalPpq_ = 0.0, ppqPerSample_ = 0.0;
    int    beatsPerBar_ = 4, lastN_ = 0;
    bool   playing_ = false, wasPlaying_ = false, internalPlaying_ = false, discontinuity_ = false;
};

} // namespace hic
