#pragma once
#include <cmath>
#include "hic/Math.h"

namespace hic {

/// One-pole lowpass / highpass. Also used as a parameter smoother.
class OnePole {
public:
    void setLp(float hz, float sr) { a_ = std::exp(-kTwoPi * clamp(hz, 0.1f, sr * 0.49f) / sr); }
    void setTimeMs(float ms, float sr) { a_ = smoothCoef(ms, sr); }
    void setCoef(float a) { a_ = a; }
    void reset(float v = 0.0f) { y_ = v; }
    float lp(float x) { y_ += (1.0f - a_) * (x - y_); return y_; }
    float hp(float x) { return x - lp(x); }
    float state() const { return y_; }
private:
    float a_ = 0.0f, y_ = 0.0f;
};

/// Topology-preserving state variable filter (Zavalishin / Cytomic form).
/// Coefficients are computed in set(); tick() is a handful of multiplies.
class TptSvf {
public:
    struct Out { float lp, bp, hp; };

    void set(float hz, float q, float sr) {
        hz = clamp(hz, 1.0f, sr * 0.45f);
        q  = clamp(q, 0.1f, 40.0f);
        g_ = std::tan(kPi * hz / sr);
        k_ = 1.0f / q;
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
    void reset() { ic1_ = ic2_ = 0.0f; }

    Out tick(float x) {
        const float v3 = x - ic2_;
        const float v1 = a1_ * ic1_ + a2_ * v3;
        const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        return { v2, v1, x - k_ * v1 - v2 };
    }
    float lp(float x) { return tick(x).lp; }
    float bp(float x) { return tick(x).bp; }
    float hp(float x) { return tick(x).hp; }

private:
    float g_ = 0.0f, k_ = 1.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
    float ic1_ = 0.0f, ic2_ = 0.0f;
};

/// Two-pole resonator tuned by frequency and T60. A unit impulse yields a
/// decaying sine of amplitude ~1. Partials above 0.45 sr are muted.
/// This is the building block of the modal voices: three multiplies each.
class Resonator {
public:
    void set(float hz, float t60Sec, float sr) {
        if (hz <= 0.0f || hz > sr * 0.45f || t60Sec <= 0.0f) { inGain_ = 0.0f; a1_ = a2_ = 0.0f; return; }
        const float w = kTwoPi * hz / sr;
        const float r = std::exp(-6.9077553f / (t60Sec * sr));
        a1_ = 2.0f * r * std::cos(w);
        a2_ = -r * r;
        inGain_ = std::sin(w);
    }
    void reset() { y1_ = y2_ = 0.0f; }
    bool muted() const { return inGain_ == 0.0f; }

    float tick(float x) {
        const float y = x * inGain_ + a1_ * y1_ + a2_ * y2_;
        y2_ = y1_;
        y1_ = y;
        return y;
    }
    /// True once the ringing has died away.
    bool quiet() const { return std::fabs(y1_) < 1e-6f && std::fabs(y2_) < 1e-6f; }

private:
    float a1_ = 0.0f, a2_ = 0.0f, inGain_ = 0.0f;
    float y1_ = 0.0f, y2_ = 0.0f;
};

} // namespace hic
