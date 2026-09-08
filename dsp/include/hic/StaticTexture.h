#pragma once
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"

namespace hic {

enum class StaticClock : uint8_t { Off = 0, Sixteenths, Eighths, Quarters, Steps };

/// The clocked static voice: dust clicks and filtered hiss that only exist
/// inside rhythmic pulses. There is no free-running mode.
struct StaticParams {
    float levelDetail = 0.6f;    // scaled by the bus Texture knob
    float density     = 60.0f;   // dust events per second while a pulse is open
    float colour      = 0.3f;    // 0 dust clicks .. 1 filtered hiss
    float warmth      = 6000.0f; // lowpass on the dust
    StaticClock clock = StaticClock::Sixteenths;
    float pulseMs     = 40.0f;   // pulse length on the clock divisions
    float attackMs    = 2.0f;
    float releaseMs   = 30.0f;
};

class StaticTexture {
public:
    void prepare(float sr, uint32_t seed = 99u) {
        sr_ = sr; seed_ = seed;
        for (int c = 0; c < 2; ++c) { pend_[c] = 0.0f; pink_[c] = {}; warm_[c].reset(); tilt_[c].reset(); }
        env_.reset(0.0f); open_ = false; pulseIndex_ = -1;
    }

    /// Per-sample pulse gate. `pulseIndex` (-1 = closed) reseeds the noise on each new
    /// pulse so a bar replays identically.
    void gate(int64_t pulseIndex, uint32_t seedIndex) {
        const bool open = pulseIndex >= 0;
        if (open && (!open_ || pulseIndex != pulseIndex_)) {
            pulseIndex_ = pulseIndex;
            for (int c = 0; c < 2; ++c) {
                rng_[c].seed(hashSeed(seed_, seedIndex, static_cast<uint32_t>(c) + 1u));
                pend_[c] = 0.0f; pink_[c] = {}; warm_[c].reset(); tilt_[c].reset();
            }
        }
        open_ = open;
        env_.gate(open_);
    }
    bool gateOpen() const { return open_; }

    /// Adds the static into L/R. `duck` is a per-sample gain multiplier (may be null);
    /// `pulse` holds the per-sample pulse index (-1 = closed).
    void render(float* L, float* R, const float* duck, const int64_t* pulse, const uint32_t* pulseSeed, int n, const StaticParams& p, float level) {
        env_.setAttackMs(p.attackMs, sr_);
        env_.setReleaseMs(p.releaseMs, sr_);
        if (p.clock == StaticClock::Off || level <= 0.0f) { gate(-1, 0u); return; }
        bool any = env_.active();
        for (int i = 0; i < n && !any; ++i) any = pulse[i] >= 0;
        if (!any) return;
        const float pEvent = clamp(p.density, 1.0f, 2000.0f) / sr_;
        const float dustMix = 1.0f - p.colour, hissMix = p.colour * 0.12f;
        warm_[0].setLp(p.warmth, sr_); warm_[1].setLp(p.warmth, sr_);
        const float tiltHz = mapExpInline(p.colour, 2000.0f, 12000.0f);
        tilt_[0].setLp(tiltHz, sr_); tilt_[1].setLp(tiltHz, sr_);

        for (int i = 0; i < n; ++i) {
            gate(pulse[i], pulseSeed[i]);
            const float lvl = level * env_.tick() * (duck ? duck[i] : 1.0f);
            for (int c = 0; c < 2; ++c) {
                Rng& r = rng_[c];
                float imp = pend_[c]; pend_[c] = 0.0f;
                if (r.uniform() < pEvent) {
                    const float a = std::exp(-3.0f * r.uniform()) * (r.chance(0.5f) ? 1.0f : -1.0f);
                    imp += a; pend_[c] = -a * 0.6f;
                }
                const float w = r.bipolar();
                Pink& k = pink_[c];
                k.b0 = 0.99765f * k.b0 + w * 0.0990460f;
                k.b1 = 0.96300f * k.b1 + w * 0.2965164f;
                k.b2 = 0.57000f * k.b2 + w * 1.0526913f;
                const float pk = k.b0 + k.b1 + k.b2 + w * 0.1848f;
                const float x = warm_[c].lp(imp) * 2.5f * dustMix + tilt_[c].lp(pk) * hissMix;
                (c == 0 ? L : R)[i] += x * lvl;
            }
        }
        for (int c = 0; c < 2; ++c) { warm_[c].flush(); tilt_[c].flush(); }
    }

private:
    static float mapExpInline(float m, float lo, float hi) { return lo * std::exp(m * std::log(hi / lo)); }
    struct Pink { float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f; };

    float sr_ = 48000.0f;
    uint32_t seed_ = 99u;
    Rng rng_[2];
    float pend_[2] = {};
    OnePole warm_[2], tilt_[2];
    Pink pink_[2];
    ArEnv env_;
    bool open_ = false;
    int64_t pulseIndex_ = -1;
};

} // namespace hic
