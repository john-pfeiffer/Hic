#pragma once
#include "hic/Math.h"
#include "hic/Filters.h"

namespace hic {

/// The bus Damp knob: the blanket. A gentle lowpass plus a tilt that
/// lifts the lows and lowers the highs as it closes.
class Damp {
public:
    void prepare(float sr) { sr_ = sr; for (auto& t : tilt_) { t.setLp(1000.0f, sr); t.reset(); } for (auto& l : lp_) l.reset(); }
    void set(float damp) {
        damp = clamp(damp, 0.0f, 1.0f);
        on_ = damp >= 0.001f;
        const float fc = mapExpInline(1.0f - damp, 700.0f, 20000.0f);
        lpOn_ = fc < 19000.0f;
        for (auto& l : lp_) l.set(fc, 0.5f, sr_);
        loGain_ = 1.0f + 0.25f * damp;
        hiGain_ = 1.0f - 0.5f * damp;
    }
    void process(float* L, float* R, int n) {
        if (!on_) return;
        for (int i = 0; i < n; ++i) { L[i] = one(0, L[i]); R[i] = one(1, R[i]); }
        for (auto& t : tilt_) t.flush();
        for (auto& l : lp_) l.flush();
    }
private:
    static float mapExpInline(float m, float lo, float hi) { return lo * std::exp(m * std::log(hi / lo)); }
    float one(int c, float x) {
        const float lo = tilt_[c].lp(x);
        float y = lo * loGain_ + (x - lo) * hiGain_;
        if (lpOn_) y = lp_[c].lp(y);
        return y;
    }
    float sr_ = 48000.0f, loGain_ = 1.0f, hiGain_ = 1.0f;
    bool on_ = false, lpOn_ = false;
    OnePole tilt_[2];
    TptSvf lp_[2];
};

} // namespace hic
