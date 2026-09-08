#pragma once
#include <cmath>
#include "hic/Filters.h"

namespace hic {

/// Bit depth and sample rate reducer in the SP-1200 / SP-303 spirit.
/// `warm` blends a pre-lowpass (anti-alias) and a post-smoother so the
/// result reads as grainy tape rather than chiptune.
class Redux {
public:
    void prepare(float sr) { sr_ = sr; set(16, 1, 0.0f); }

    void set(int bits, int rateDiv, float warm) {
        bits_ = clamp(bits, 2, 24);
        rateDiv_ = clamp(rateDiv, 1, 64);
        warm_ = clamp(warm, 0.0f, 1.0f);
        levels_ = std::exp2(static_cast<float>(bits_ - 1));
        invLevels_ = 1.0f / levels_;
        const float fc = 0.4f * sr_ / static_cast<float>(rateDiv_);
        pre_.setLp(fc, sr_);
        post_.setLp(fc, sr_);
    }
    /// Convenience: one 0..1 knob from clean to 10-bit at 1/4 rate.
    void setAmount(float amount) {
        amount = clamp(amount, 0.0f, 1.0f);
        set(16 - static_cast<int>(amount * 6.0f + 0.5f), 1 + static_cast<int>(amount * 3.0f + 0.5f), 0.7f);
    }
    void reset() { count_ = 0; held_ = 0.0f; pre_.reset(); post_.reset(); }
    bool bypassed() const { return bits_ >= 16 && rateDiv_ == 1; }

    float tick(float x) {
        if (bypassed()) return x;
        const float pre = pre_.lp(x);
        const float in = x + (pre - x) * warm_;
        if (++count_ >= rateDiv_) {
            count_ = 0;
            held_ = std::floor(in * levels_ + 0.5f) * invLevels_;
        }
        const float sm = post_.lp(held_);
        return held_ + (sm - held_) * warm_;
    }

private:
    float sr_ = 48000.0f;
    int bits_ = 16, rateDiv_ = 1, count_ = 0;
    float warm_ = 0.0f, levels_ = 32768.0f, invLevels_ = 1.0f / 32768.0f, held_ = 0.0f;
    OnePole pre_, post_;
};

} // namespace hic
