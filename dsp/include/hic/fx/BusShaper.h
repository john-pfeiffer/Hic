#pragma once
#include "hic/Math.h"

namespace hic {

/// The bus Drive knob: a gentle stereo wavefold and saturation.
class BusShaper {
public:
    void set(float drive) {
        drive = clamp(drive, 0.0f, 1.0f);
        on_ = drive >= 0.001f;
        // Gentle by design: at full Drive a 0 dBFS peak folds once and the knee is soft.
        foldG_ = 1.0f + 0.8f * drive;
        satG_ = 1.0f + 1.2f * drive;
        satNorm_ = 1.0f / fastTanh(satG_);
        mix_ = smoothstep(drive, 0.0f, 0.5f);
        trim_ = 1.0f / (1.0f + 0.25f * drive);
    }
    void process(float* L, float* R, int n) const {
        if (!on_) return;
        for (int i = 0; i < n; ++i) { L[i] = shape(L[i]); R[i] = shape(R[i]); }
    }
private:
    float shape(float x) const {
        float y = foldTri(x * foldG_);
        y = fastTanh(y * satG_) * satNorm_;
        return (x + (y - x) * mix_) * trim_;
    }
    bool on_ = false;
    float foldG_ = 1.0f, satG_ = 1.0f, satNorm_ = 1.0f, mix_ = 0.0f, trim_ = 1.0f;
};

} // namespace hic
