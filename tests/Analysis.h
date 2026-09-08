#pragma once
// Signal statistics used by tests and the render tool.
#include <cmath>
#include <algorithm>
#include "hic/Filters.h"
#include "hic/Math.h"
#include <vector>

namespace hictest {

inline float peak(const float* x, int n) { float p = 0.0f; for (int i = 0; i < n; ++i) p = std::max(p, std::fabs(x[i])); return p; }
inline float peakDb(const float* x, int n) { return hic::gainToDb(peak(x, n)); }
inline float rms(const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += double(x[i]) * double(x[i]); return n ? float(std::sqrt(s / n)) : 0.0f; }
inline bool hasNaN(const float* x, int n) { for (int i = 0; i < n; ++i) if (!(x[i] == x[i]) || std::isinf(x[i])) return true; return false; }

inline int firstIndexAbove(const float* x, int n, float thr) { for (int i = 0; i < n; ++i) if (std::fabs(x[i]) > thr) return i; return -1; }
inline int lastIndexAbove(const float* x, int n, float thr) { for (int i = n - 1; i >= 0; --i) if (std::fabs(x[i]) > thr) return i; return -1; }

/// Milliseconds from the first sample above -60 dB (relative to the peak)
/// to the last one. A rough "how long does this sound last".
inline float decayMs(const float* x, int n, float sr, float dbDown = -60.0f) {
    const float thr = peak(x, n) * hic::dbToGain(dbDown);
    if (thr <= 0.0f) return 0.0f;
    const int a = firstIndexAbove(x, n, thr), b = lastIndexAbove(x, n, thr);
    return a < 0 ? 0.0f : 1000.0f * float(b - a + 1) / sr;
}

/// Energy above and below fc (4th-order split). Returns high/low in dB.
inline float bandRatioDb(const float* x, int n, float sr, float fc) {
    hic::TptSvf l1, l2, h1, h2;
    l1.set(fc, 0.707f, sr); l2.set(fc, 0.707f, sr); h1.set(fc, 0.707f, sr); h2.set(fc, 0.707f, sr);
    double lo = 0, hi = 0;
    for (int i = 0; i < n; ++i) {
        const float l = l2.lp(l1.lp(x[i]));
        const float h = h2.hp(h1.hp(x[i]));
        lo += double(l) * double(l); hi += double(h) * double(h);
    }
    if (lo <= 1e-20) return 200.0f;
    if (hi <= 1e-20) return -200.0f;
    return float(10.0 * std::log10(hi / lo));
}

/// Fundamental estimate by normalised autocorrelation over the first 60 ms after
/// the onset (searching 25 Hz .. 4 kHz). Returns 0 when nothing periodic is found.
inline float estimateF0(const float* x, int n, float sr, float fromMs = 3.0f, float windowMs = 60.0f) {
    const int onset = firstIndexAbove(x, n, peak(x, n) * 0.05f);
    if (onset < 0) return 0.0f;
    const int from = onset + int(fromMs * 0.001f * sr);
    const int len = int(windowMs * 0.001f * sr);
    if (from + 2 * len >= n) return 0.0f;
    const int minLag = int(sr / 4000.0f), maxLag = int(sr / 25.0f);
    double e0 = 0.0;
    for (int i = 0; i < len; ++i) e0 += double(x[from + i]) * double(x[from + i]);
    if (e0 < 1e-12) return 0.0f;
    std::vector<float> corr(size_t(maxLag + 1), -1.0f);
    for (int lag = minLag; lag <= maxLag && from + len + lag < n; ++lag) {
        double c = 0.0, e1 = 0.0;
        for (int i = 0; i < len; ++i) { c += double(x[from + i]) * double(x[from + i + lag]); e1 += double(x[from + i + lag]) * double(x[from + i + lag]); }
        corr[size_t(lag)] = e1 > 1e-12 ? float(c / std::sqrt(e0 * e1)) : 0.0f;
    }
    // Smooth signals correlate at tiny lags: only look past the first dip below zero.
    int start = minLag;
    while (start <= maxLag && corr[size_t(start)] > 0.0f) ++start;
    float best = 0.0f; int bestLag = 0;
    for (int lag = start; lag <= maxLag; ++lag) if (corr[size_t(lag)] > best) { best = corr[size_t(lag)]; bestLag = lag; }
    // Prefer the shortest lag past the dip whose correlation is close to the best (avoids octave-down errors).
    for (int lag = start; lag < bestLag; ++lag) if (corr[size_t(lag)] > best * 0.9f && corr[size_t(lag)] > 0.5f) { bestLag = lag; break; }
    if (best <= 0.3f || bestLag <= 0) return 0.0f;
    // Parabolic interpolation around the peak.
    float lagF = float(bestLag);
    if (bestLag > start && bestLag < maxLag) {
        const float a = corr[size_t(bestLag - 1)], b = corr[size_t(bestLag)], c = corr[size_t(bestLag + 1)];
        const float den = a - 2.0f * b + c;
        if (std::fabs(den) > 1e-9f) lagF += 0.5f * (a - c) / den;
    }
    return sr / lagF;
}

/// Count distinct sample values (used to verify bit reduction).
inline int distinctValues(const float* x, int n, float* scratch) {
    std::copy(x, x + n, scratch);
    std::sort(scratch, scratch + n);
    int d = n > 0 ? 1 : 0;
    for (int i = 1; i < n; ++i) if (scratch[i] != scratch[i - 1]) ++d;
    return d;
}

} // namespace hictest
