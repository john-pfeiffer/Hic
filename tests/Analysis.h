#pragma once
// Signal statistics used by tests and the render tool.
#include <cmath>
#include <algorithm>
#include "hic/Filters.h"
#include "hic/Math.h"

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

/// Count distinct sample values (used to verify bit reduction).
inline int distinctValues(const float* x, int n, float* scratch) {
    std::copy(x, x + n, scratch);
    std::sort(scratch, scratch + n);
    int d = n > 0 ? 1 : 0;
    for (int i = 1; i < n; ++i) if (scratch[i] != scratch[i - 1]) ++d;
    return d;
}

} // namespace hictest
