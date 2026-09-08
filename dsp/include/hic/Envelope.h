#pragma once
#include "hic/Math.h"

namespace hic {

/// Exponential decay: trigger() sets the level, each tick multiplies by k.
/// Decay time is the time to reach -60 dB.
class ExpDecay {
public:
    void setDecayMs(float ms, float sr) { k_ = decayCoef(ms, sr); }
    void trigger(float level = 1.0f) { y_ = level; }
    void kill() { y_ = 0.0f; }
    float tick() { const float o = y_; y_ *= k_; if (y_ < 1e-9f) y_ = 0.0f; return o; }
    float value() const { return y_; }
    bool active() const { return y_ > 1e-5f; }
private:
    float k_ = 0.0f, y_ = 0.0f;
};

/// Attack / release envelope driven by a gate. One-pole toward 1 or 0.
class ArEnv {
public:
    /// Attack reaches ~99 % of full scale after `ms`.
    void setAttackMs(float ms, float sr)  { ca_ = smoothCoef((ms < 0.1f ? 0.1f : ms) / 5.0f, sr); }
    /// Release reaches -60 dB after `ms` (same convention as ExpDecay).
    void setReleaseMs(float ms, float sr) { cr_ = decayCoef(ms < 0.1f ? 0.1f : ms, sr); }
    void gate(bool on) { gate_ = on; }
    bool gated() const { return gate_; }
    void reset(float v = 0.0f) { y_ = v; }
    float tick() {
        const float target = gate_ ? 1.0f : 0.0f;
        const float c = gate_ ? ca_ : cr_;
        y_ += (1.0f - c) * (target - y_);
        if (!gate_ && y_ < 1e-5f) y_ = 0.0f;
        return y_;
    }
    float value() const { return y_; }
    bool active() const { return gate_ || y_ > 0.0f; }
private:
    float ca_ = 0.0f, cr_ = 0.0f, y_ = 0.0f;
    bool gate_ = false;
};

} // namespace hic
