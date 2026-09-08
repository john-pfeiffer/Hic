#pragma once
#include <cstdint>
#include "hic/Config.h"

namespace hic {

constexpr int kMaxSteps = 64;
constexpr int kNumPatterns = 8;

enum StepFlags : uint8_t {
    StepAccent  = 1 << 0,
    StepReverse = 1 << 1,   // this hit plays backwards, swelling into its step
    StepBedGate = 1 << 2,   // the bed is open during this step (bed gate mode "Steps")
};

/// One step. Eight bytes, plain data, so a pattern is a small memcpy.
struct Step {
    uint8_t on         = 0;
    uint8_t vel        = 100;   // 1..127
    int8_t  nudgeMs    = 0;     // -20..20
    uint8_t prob       = 100;   // 0..100 %
    uint8_t ratchet    = 0;     // 0 or 1 = single hit, 2..4 = sub-hits inside the step
    uint8_t flags      = 0;     // StepFlags
    int8_t  noteOffset = 0;     // semitones from the pad's base note (melodic pads)
    uint8_t reserved   = 0;
};

/// One track drives one pad. Tracks can have different lengths (polymeter).
struct Track {
    uint8_t pad      = 0;
    uint8_t length   = 16;      // 1..64 steps
    uint8_t mute     = 0;
    uint8_t reserved = 0;
    Step    steps[kMaxSteps];
};

struct Pattern {
    Track   tracks[kNumPads];
    uint8_t stepsPerBeat = 4;   // 16ths
    uint8_t swingPct     = 50;  // 50 straight .. 75 heavy (MPC style, applies to odd steps)
    uint8_t reserved[2]  = { 0, 0 };
};

/// Empty pattern with each track assigned to its pad.
void clearPattern(Pattern& p);

/// A restrained demo: kick, rimshot, clicks, shaker, a few glock notes, one reversed hat.
void makeDemoPattern(Pattern& p);

} // namespace hic
