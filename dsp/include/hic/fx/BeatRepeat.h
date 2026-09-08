#pragma once
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/seq/Clock.h"

namespace hic {

struct RepeatParams {
    bool  enabled        = true;
    int   grid           = 32;      // slice = 1/grid note: 16, 32, 64
    float probability    = 0.10f;   // chance per bar that a stutter happens
    float lengthBeats    = 0.5f;    // how long the stutter runs
    int   minBarsBetween = 2;       // never two stutters closer than this
    float mix            = 1.0f;    // 0 dry .. 1 full replace
};

/// A stutter that happens rarely: at a grid boundary, with a seeded roll, the
/// last grid slice is frozen and looped for a short while, replacing the live
/// signal. No pitch decay, no filter sweep; restraint is the whole point.
class BeatRepeat {
public:
    static constexpr int kSlice = nextPow2(samplesForMs(320.0f));   // 1/16 at 60 BPM plus margin

    void prepare(float sr) {
        sr_ = sr;
        for (int c = 0; c < 2; ++c) { ring_[c].clear(); for (auto& s : slice_[c]) s = 0.0f; }
        repeating_ = false; lastGridIdx_ = -1; lastRepeatBar_ = -1000000; count_ = 0; force_ = false;
        fade_ = static_cast<int>(0.001f * sr) + 1;
    }

    void force() { force_ = true; }
    int  repeatCount() const { return count_; }
    bool repeating() const { return repeating_; }
    int64_t lastRepeatBar() const { return lastRepeatBar_; }

    void process(float* L, float* R, int n, const RepeatParams& p, const Clock& clock, uint32_t seed) {
        const int grid = clamp(p.grid, 4, 128);
        const double gridBeats = 4.0 / grid;
        const double spb = clock.samplesPerBeat();
        const int sliceLen = clamp(static_cast<int>(gridBeats * spb), 8, kSlice);
        const int repeatLen = static_cast<int>(clamp(static_cast<double>(p.lengthBeats), 0.05, 4.0) * spb);
        const bool timed = clock.playing() && p.enabled;

        for (int i = 0; i < n; ++i) {
            const float inL = L[i], inR = R[i];
            ring_[0].write(inL); ring_[1].write(inR);

            if (!repeating_) {
                bool start = false;
                if (timed) {
                    const double ppq = clock.ppqAt(i);
                    const int64_t gridIdx = static_cast<int64_t>(std::floor(ppq / gridBeats + 1e-9));
                    if (gridIdx != lastGridIdx_) {
                        lastGridIdx_ = gridIdx;
                        const int64_t bar = static_cast<int64_t>(std::floor(ppq / clock.beatsPerBar() + 1e-9));
                        if (bar - lastRepeatBar_ >= p.minBarsBetween) {
                            const double perGrid = clamp(static_cast<double>(p.probability), 0.0, 1.0) * gridBeats / static_cast<double>(clock.beatsPerBar());
                            Rng r(hashSeed(seed, static_cast<uint32_t>(gridIdx & 0xffffffff), 0x5eedu));
                            if (static_cast<double>(r.uniform()) < perGrid) { start = true; lastRepeatBar_ = bar; }
                        }
                    }
                }
                if (force_) { start = true; force_ = false; }
                if (start) {
                    // Freeze the last slice (including this sample).
                    sliceLen_ = sliceLen;
                    for (int c = 0; c < 2; ++c)
                        for (int k = 0; k < sliceLen_; ++k) slice_[c][k] = ring_[c].read(sliceLen_ - 1 - k);
                    repeating_ = true; pos_ = 0; total_ = repeatLen; ++count_;
                }
            }

            if (repeating_) {
                const int k = pos_ % sliceLen_;
                float w = 1.0f;                              // loop-point crossfade
                if (k < fade_) w = static_cast<float>(k) / static_cast<float>(fade_);
                else if (k >= sliceLen_ - fade_) w = static_cast<float>(sliceLen_ - k) / static_cast<float>(fade_);
                float m = p.mix;                             // dry/repeat ramp at the edges
                if (pos_ < fade_) m *= static_cast<float>(pos_) / static_cast<float>(fade_);
                else if (pos_ >= total_ - fade_) m *= static_cast<float>(total_ - pos_) / static_cast<float>(fade_);
                L[i] = inL + (slice_[0][k] * w - inL) * m;
                R[i] = inR + (slice_[1][k] * w - inR) * m;
                if (++pos_ >= total_) repeating_ = false;
            }
        }
    }

private:
    float sr_ = 48000.0f;
    bool repeating_ = false, force_ = false;
    int sliceLen_ = 8, pos_ = 0, total_ = 0, fade_ = 48, count_ = 0;
    int64_t lastGridIdx_ = -1, lastRepeatBar_ = -1000000;
    DelayLine<kSlice> ring_[2];
    float slice_[2][kSlice] = {};
};

} // namespace hic
