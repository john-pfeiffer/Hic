#pragma once
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"

namespace hic {

enum class BedType : uint8_t { Off = 0, Crackle, Hiss, Both };
enum class BedGate : uint8_t { Off = 0, Sixteenths, Eighths, Quarters, Steps };

struct BedParams {
    BedType type          = BedType::Crackle;
    float   level         = 0.25f;    // 0..1
    float   density       = 40.0f;    // crackle events per second, 5..200
    float   warmth        = 6000.0f;  // crackle lowpass, 2k..10k
    float   popRatio      = 0.06f;    // fraction of events that are bigger pops
    float   color         = 0.5f;     // hiss tilt 0 (dark) .. 1 (bright)
    float   hissLevel     = 0.5f;     // hiss relative to crackle when Both
    BedGate gate          = BedGate::Off;
    float   gateAttackMs  = 4.0f;
    float   gateReleaseMs = 60.0f;
    float   gateDuty      = 0.5f;     // fraction of the gate period that is open
};

/// Continuous vinyl crackle and tape hiss, stereo, optionally gated into a
/// rhythm by the clock or the sequencer. Lives on the bus, not on a pad.
class Bed {
public:
    void prepare(float sr, uint32_t seed = 99u) {
        sr_ = sr;
        for (int c = 0; c < 2; ++c) {
            rng_[c].seed(hashSeed(seed, static_cast<uint32_t>(c) + 1u));
            pend_[c] = 0.0f; popLp_[c].setLp(600.0f, sr); popLp_[c].reset();
            pink_[c] = {};
        }
        levelSm_.setTimeMs(20.0f, sr); levelSm_.reset(0.0f);
        gateEnv_.reset(1.0f);
        gateOpen_ = true;
    }

    void setGate(bool open) { gateOpen_ = open; }

    /// Adds the bed into L/R. `duck` is a per-sample gain multiplier (may be null).
    void render(float* L, float* R, const float* duck, int n, const BedParams& p) {
        if (p.type == BedType::Off) { levelSm_.reset(0.0f); return; }
        const bool crackle = p.type == BedType::Crackle || p.type == BedType::Both;
        const bool hiss    = p.type == BedType::Hiss    || p.type == BedType::Both;
        const float pEvent = clamp(p.density, 1.0f, 500.0f) / sr_;
        const float hissGain = (p.type == BedType::Both ? p.hissLevel : 1.0f) * 0.08f;
        warm_[0].setLp(p.warmth, sr_); warm_[1].setLp(p.warmth, sr_);
        const float tiltHz = mapExpInline(p.color, 800.0f, 14000.0f);
        tilt_[0].setLp(tiltHz, sr_); tilt_[1].setLp(tiltHz, sr_);
        gateEnv_.setAttackMs(p.gateAttackMs, sr_);
        gateEnv_.setReleaseMs(p.gateReleaseMs, sr_);
        gateEnv_.gate(p.gate == BedGate::Off ? true : gateOpen_);

        for (int i = 0; i < n; ++i) {
            const float lvl = levelSm_.lp(p.level) * gateEnv_.tick() * (duck ? duck[i] : 1.0f);
            float out[2];
            for (int c = 0; c < 2; ++c) {
                Rng& r = rng_[c];
                float x = 0.0f;
                if (crackle) {
                    float imp = pend_[c]; pend_[c] = 0.0f;
                    if (r.uniform() < pEvent) {
                        const float a = std::exp(-3.0f * r.uniform()) * (r.chance(0.5f) ? 1.0f : -1.0f); // log-uniform
                        if (r.uniform() < p.popRatio) popImp_[c] = a * 4.0f;
                        else { imp += a; pend_[c] = -a * 0.6f; }
                    }
                    float pop = popLp_[c].lp(popImp_[c]) * 3.0f; popImp_[c] = 0.0f;
                    x += warm_[c].lp(imp) * 2.5f + pop;
                }
                if (hiss) {
                    // Paul Kellet's economy pink filter, then a colour tilt.
                    const float w = r.bipolar();
                    Pink& k = pink_[c];
                    k.b0 = 0.99765f * k.b0 + w * 0.0990460f;
                    k.b1 = 0.96300f * k.b1 + w * 0.2965164f;
                    k.b2 = 0.57000f * k.b2 + w * 1.0526913f;
                    const float pk = k.b0 + k.b1 + k.b2 + w * 0.1848f;
                    x += tilt_[c].lp(pk) * hissGain;
                }
                out[c] = x * lvl;
            }
            L[i] += out[0];
            R[i] += out[1];
        }
    }

private:
    static float mapExpInline(float m, float lo, float hi) { return lo * std::exp(m * std::log(hi / lo)); }
    struct Pink { float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f; };

    float sr_ = 48000.0f;
    Rng rng_[2];
    float pend_[2] = {}, popImp_[2] = {};
    OnePole warm_[2], tilt_[2], popLp_[2], levelSm_;
    Pink pink_[2];
    ArEnv gateEnv_;
    bool gateOpen_ = true;
};

} // namespace hic
