#pragma once
#include "hic/Envelope.h"
#include "hic/Math.h"

namespace hic {

struct DuckParams {
    float depthDb   = -14.0f;   // how far the bed drops on a kick
    float attackMs  = 2.0f;
    float holdMs    = 60.0f;    // stays down for the body of the kick
    float releaseMs = 160.0f;
};

/// Trigger-driven ducker for the bed: a hit from a duck-source pad pulls the
/// gain down, and it recovers over the release. Deterministic, one multiply.
class Ducker {
public:
    void prepare(float sr) { sr_ = sr; env_.reset(0.0f); }
    void set(const DuckParams& p) {
        depthGain_ = dbToGain(clamp(p.depthDb, -60.0f, 0.0f));
        env_.setAttackMs(p.attackMs, sr_);
        env_.setReleaseMs(p.releaseMs, sr_);
        hold_ = static_cast<int>((p.attackMs + p.holdMs) * 0.001f * sr_) + 1;
    }
    void trigger() { env_.gate(true); holdLeft_ = hold_; }
    float tick() {
        if (holdLeft_ > 0 && --holdLeft_ == 0) env_.gate(false);
        return 1.0f - (1.0f - depthGain_) * env_.tick();
    }
    void render(float* gain, int n) { for (int i = 0; i < n; ++i) gain[i] = tick(); }

private:
    float sr_ = 48000.0f, depthGain_ = 0.2f;
    int hold_ = 0, holdLeft_ = 0;
    ArEnv env_;
};

} // namespace hic
