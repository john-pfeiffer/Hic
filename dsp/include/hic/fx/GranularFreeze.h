#pragma once
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/DelayLine.h"

namespace hic {

struct FreezeParams {
    bool  hold            = false;
    float grainMs         = 18.0f;   // 4..40
    float density         = 30.0f;   // grains per second, 5..200
    float sprayMs         = 25.0f;   // random read-position spread
    float pitchJitterCents = 0.0f;   // 0..100
    float mix             = 0.6f;    // return level
};

/// Records the freeze tap (hats and clicks by default). While held, a small
/// pool of Hann-windowed grains plays from a fixed spot in the recording.
/// Sparse and short = ticking insects; dense = a frozen texture.
class GranularFreeze {
public:
    static constexpr int kRing = nextPow2(samplesForMs(500.0f));
    static constexpr int kGrains = 8;

    void prepare(float sr, uint32_t seed = 7u) {
        sr_ = sr; ring_.clear(); rng_.seed(seed);
        for (auto& g : grains_) g.len = 0;
        held_ = false; untilSpawn_ = 0;
    }

    bool held() const { return held_; }

    /// `tap` is recorded; grains are added to L/R.
    void process(const float* tap, float* L, float* R, int n, const FreezeParams& p) {
        const int grainLen = clamp(static_cast<int>(p.grainMs * 0.001f * sr_), 16, kRing / 4);
        const int spray = static_cast<int>(p.sprayMs * 0.001f * sr_);
        const int spawnInterval = static_cast<int>(sr_ / clamp(p.density, 1.0f, 400.0f));
        const float jitter = p.pitchJitterCents * (1.0f / 1200.0f);

        for (int i = 0; i < n; ++i) {
            const bool holdNow = p.hold || external_;
            // Track the most recent onset in the tap so a freeze lands on a
            // click rather than on the silence after it.
            const float a = std::fabs(tap[i]);
            if (a > kOnsetThreshold) {
                if (quiet_ > kOnsetGap) onsetPos_ = ring_.writePos();
                quiet_ = 0;
            } else if (quiet_ < (1 << 30)) ++quiet_;
            if (holdNow && !held_) {
                const int sinceOnset = ring_.writePos() - onsetPos_;
                anchor_ = (onsetPos_ >= 0 && sinceOnset >= 0 && sinceOnset < kRing / 2) ? onsetPos_ : ring_.writePos() - grainLen - spray - 1;
                untilSpawn_ = 0;
            }
            held_ = holdNow;
            ring_.write(tap[i]);
            if (!held_) continue;

            if (--untilSpawn_ <= 0) {
                untilSpawn_ = spawnInterval;
                for (auto& g : grains_) {
                    if (g.len > 0) continue;
                    g.len = grainLen; g.age = 0;
                    // Each grain contains the anchor somewhere inside its window,
                    // so a short click is heard at a different loudness every time.
                    const int start = anchor_ - static_cast<int>(rng_.uniform() * static_cast<float>(grainLen - 2)) - 1
                                    + static_cast<int>(rng_.bipolar() * static_cast<float>(spray));
                    g.pos = static_cast<float>(start);
                    g.inc = std::exp2(rng_.bipolar() * jitter);
                    g.pan = rng_.uniform();
                    break;
                }
            }
            float mixL = 0.0f, mixR = 0.0f;
            for (auto& g : grains_) {
                if (g.len <= 0) continue;
                const float w = fastSin01(0.5f * static_cast<float>(g.age) / static_cast<float>(g.len));   // sin(pi t) window
                const int   ip = static_cast<int>(g.pos);
                const float fr = g.pos - static_cast<float>(ip);
                const float s = ring_.at(ip) + (ring_.at(ip + 1) - ring_.at(ip)) * fr;
                const float v = s * w * w;
                mixL += v * (1.0f - g.pan); mixR += v * g.pan;
                g.pos += g.inc;
                if (++g.age >= g.len) g.len = 0;
            }
            L[i] += mixL * p.mix * 1.4f;
            R[i] += mixR * p.mix * 1.4f;
        }
    }

    /// MIDI hold note (independent of the parameter).
    void setExternalHold(bool on) { external_ = on; }

private:
    struct Grain { float pos = 0.0f, inc = 1.0f, pan = 0.5f; int len = 0, age = 0; };
    static constexpr float kOnsetThreshold = 0.004f;
    static constexpr int   kOnsetGap = 240;      // 5 ms at 48 kHz of quiet defines a new onset
    float sr_ = 48000.0f;
    bool held_ = false, external_ = false;
    int anchor_ = 0, untilSpawn_ = 0, onsetPos_ = -1, quiet_ = 1 << 30;
    DelayLine<kRing> ring_;
    Grain grains_[kGrains];
    Rng rng_;
};

} // namespace hic
