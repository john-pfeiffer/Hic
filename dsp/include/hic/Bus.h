#pragma once

namespace hic {

/// The six "eurorack glue" knobs. Macro scales, detail shapes: each one
/// drives a detailed struct elsewhere in the engine.
struct BusParams {
    float drive   = 0.15f;   // bus wavefold + saturation, gentle
    float damp    = 0.20f;   // the blanket: global lowpass and tilt
    float texture = 0.30f;   // grit inside hits and the clocked static
    float space   = 0.35f;   // spring reverb amount and size
    float drift   = 1.00f;   // multiplier on every pad's per-hit drift (0..2)
    float feel    = 0.50f;   // timing and velocity scatter amount
};

} // namespace hic
