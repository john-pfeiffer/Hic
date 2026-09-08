#pragma once
#include <cmath>
#include <cstdint>

namespace hic {

constexpr int kNumMacros = 6;

/// The six macros, identical in meaning on every pad.
enum Macro : uint8_t { MacroTune = 0, MacroDecay, MacroExciter, MacroBody, MacroBreak, MacroDrift };

enum PadFlags : uint8_t {
    PadFollowsNote  = 1 << 0,   // pitch tracks the incoming MIDI note
    PadReverse      = 1 << 1,   // play the hit backwards (swell into the beat)
    PadFreezeSource = 1 << 2,   // feeds the granular freeze tap
    PadDuckSource   = 1 << 3,   // triggers the static ducker
    PadFollowKey    = 1 << 4,   // Tune snaps to the global key
};

/// Everything that describes one pad. Plain data: six 0..1 macros plus a
/// few common controls. This is the struct a hardware panel would edit.
struct PadParams {
    uint8_t chokeGroup = 0;      // 0 = none
    uint8_t maxPoly    = 2;
    uint8_t flags      = 0;
    uint8_t baseNote   = 60;     // reference note when FollowsNote is set

    float macro[kNumMacros] = { 0.5f, 0.5f, 0.3f, 0.5f, 0.0f, 0.1f };

    float level      = 0.8f;     // 0..1
    float pan        = 0.0f;     // -1..1
    float reverbSend = 0.0f;     // 0..1
    float reverseMs  = 120.0f;   // length of a reversed hit
    float velToLevel = 0.8f;     // 0..1, how much velocity moves level
    float velToTone  = 0.5f;     // 0..1, how much velocity brightens the hit
    float scatterMul = 1.0f;     // multiplies the global timing scatter
};

inline const char* macroName(int i) {
    static const char* const names[kNumMacros] = { "Tune", "Decay", "Exciter", "Body", "Break", "Drift" };
    return (i >= 0 && i < kNumMacros) ? names[i] : "";
}

/// Macro range helpers.
inline float mapLin(float m, float lo, float hi) { return lo + (hi - lo) * m; }
inline float mapExp(float m, float lo, float hi) { return lo * std::exp(m * std::log(hi / lo)); }

constexpr float kTuneLoHz = 30.0f, kTuneHiHz = 3000.0f;
constexpr float kDecayLoMs = 5.0f, kDecayHiMs = 2000.0f;
inline float tuneToHz(float m)  { return mapExp(m, kTuneLoHz, kTuneHiHz); }
inline float decayToMs(float m) { return mapExp(m, kDecayLoMs, kDecayHiMs); }
inline float hzToTune(float hz) { return std::log(hz / kTuneLoHz) / std::log(kTuneHiHz / kTuneLoHz); }
inline float msToDecay(float ms) { return std::log(ms / kDecayLoMs) / std::log(kDecayHiMs / kDecayLoMs); }

} // namespace hic
