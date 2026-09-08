#pragma once
#include "hic/Events.h"
#include "hic/Pad.h"
#include "hic/Rng.h"
#include "hic/Math.h"

namespace hic {

struct FeelParams {
    float    nudgeMs     = 0.0f;   // constant offset for every hit, -20..20
    float    scatterMs   = 10.0f;  // timing scatter at bus.feel = 1 (ms)
    float    velScatter  = 0.25f;  // velocity scatter at bus.feel = 1 (0..1)
    float    lookaheadMs = 0.0f;   // every event is delayed by this so hits can move early
    uint32_t seed        = 1;
};

/// Moves hits a few milliseconds and a few velocity steps, deterministically
/// per (seed, event): replaying a bar gives the same feel until the seed changes.
struct Humanizer {
    static void apply(NoteEvent& e, const FeelParams& feel, const PadParams& pad, float sr) {
        Rng rng(e.seed);
        const float t = rng.gauss3();
        const float v = rng.gauss3();
        const float ms = feel.nudgeMs + feel.scatterMs * pad.scatterMul * t;
        e.sampleTime += static_cast<int64_t>(ms * 0.001f * sr);
        float vel = static_cast<float>(e.vel) * (1.0f + feel.velScatter * v);
        e.vel = static_cast<uint8_t>(clamp(vel, 1.0f, 127.0f) + 0.5f);
    }
};

} // namespace hic
