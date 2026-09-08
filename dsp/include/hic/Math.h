#pragma once
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define HIC_HAS_SSE 1
#endif

namespace hic {

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

template <typename T> inline T clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float sign(float x) { return x < 0.0f ? -1.0f : 1.0f; }

/// Rational tanh approximation, exact at 0 and at +-3, monotonic and bounded in [-1, 1].
inline float fastTanh(float x) {
    x = clamp(x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/// exp(x) via (1 + x/256)^256; plenty for envelope coefficients, never used per sample.
inline float fastExp(float x) {
    x = 1.0f + x * (1.0f / 256.0f);
    for (int i = 0; i < 8; ++i) x *= x;
    return x;
}

inline float midiToHz(float note) { return 440.0f * std::exp2((note - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain(float db)   { return std::exp2(db * 0.16609640474f); }   // 10^(db/20)
inline float gainToDb(float g)    { return g > 1e-9f ? 20.0f * std::log10(g) : -180.0f; }

/// Coefficient for a one-pole reaching -60 dB after `ms` milliseconds.
inline float decayCoef(float ms, float sr) {
    const float n = ms * 0.001f * sr;
    return n < 1.0f ? 0.0f : std::exp(-6.9077553f / n);
}

/// Coefficient for a one-pole with time constant `ms` (63% of the way in ms).
inline float smoothCoef(float ms, float sr) {
    const float n = ms * 0.001f * sr;
    return n < 1.0f ? 0.0f : std::exp(-1.0f / n);
}

/// Zero out subnormals so long tails never slow the CPU down.
inline float flushDenormal(float x) { return (std::fabs(x) < 1e-20f) ? 0.0f : x; }

/// Sets flush-to-zero / denormals-are-zero on x86 for the lifetime of the object.
/// On ARM Cortex-M the FPU is configured for flush-to-zero at startup, so this is a no-op.
struct DenormalGuard {
#ifdef HIC_HAS_SSE
    unsigned int saved;
    DenormalGuard() : saved(_mm_getcsr()) { _mm_setcsr(saved | 0x8040u); }
    ~DenormalGuard() { _mm_setcsr(saved); }
#else
    DenormalGuard() {}
    ~DenormalGuard() {}
#endif
    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;
};

} // namespace hic
