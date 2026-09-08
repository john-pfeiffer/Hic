#pragma once
#include "hic/Filters.h"

namespace hic {

/// Soft tape-style saturator: tanh with a small bias for even harmonics,
/// normalized so unity input stays near unity, then a 10 Hz DC blocker.
class Saturator {
public:
    void prepare(float sr) { dc_.setLp(10.0f, sr); setDrive(0.0f); }
    void setDrive(float dB) {
        drive_ = dbToGain(clamp(dB, 0.0f, 36.0f));
        norm_ = 1.0f / fastTanh(drive_);
        bypass_ = dB <= 0.01f;
    }
    void reset() { dc_.reset(); }

    float tick(float x) {
        if (bypass_) return x;
        const float y = fastTanh(drive_ * x + kBias) * norm_;
        return dc_.hp(y);
    }

private:
    static constexpr float kBias = 0.08f;
    float drive_ = 1.0f, norm_ = 1.0f;
    bool bypass_ = true;
    OnePole dc_;
};

} // namespace hic
