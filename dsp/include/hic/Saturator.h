#pragma once
#include "hic/Filters.h"

namespace hic {

/// Soft tape-style saturator: tanh with a small bias for even harmonics,
/// normalized so unity input stays near unity. The static offset the bias
/// would add is subtracted, so silence in is exactly silence out and there
/// is no DC step at the start of a hit.
class Saturator {
public:
    void prepare(float /*sr*/) { setDrive(0.0f); }
    void setDrive(float dB) {
        drive_ = dbToGain(clamp(dB, 0.0f, 36.0f));
        norm_ = 1.0f / fastTanh(drive_);
        offset_ = fastTanh(kBias) * norm_;
        bypass_ = dB <= 0.01f;
    }
    void reset() {}

    float tick(float x) const {
        if (bypass_) return x;
        return fastTanh(drive_ * x + kBias) * norm_ - offset_;
    }

private:
    static constexpr float kBias = 0.08f;
    float drive_ = 1.0f, norm_ = 1.0f, offset_ = 0.0f;
    bool bypass_ = true;
};

} // namespace hic
