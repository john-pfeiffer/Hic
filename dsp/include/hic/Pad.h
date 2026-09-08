#pragma once
#include <cstdint>

namespace hic {

/// Which synthesis engine a pad uses.
enum class PadType : uint8_t { Kick = 0, Click, Modal, Noise, Ping, Count };

enum PadFlags : uint8_t {
    PadFollowsNote  = 1 << 0,   // pitch tracks the incoming MIDI note
    PadReverse      = 1 << 1,   // play the hit backwards (swell into the beat)
    PadFreezeSource = 1 << 2,   // feeds the granular freeze tap
    PadDuckSource   = 1 << 3,   // triggers the bed ducker
};

constexpr int kNumMacros = 8;

/// Kick attack presets.
enum KickPreset : uint8_t { KickSnap = 0, KickBlanket, KickPlain, KickPresetCount };
/// Click modes.
enum ClickPreset : uint8_t { ClickImpulse = 0, ClickDipole, ClickBurst, ClickCdSkip, ClickPresetCount };
/// Noise textures.
enum NoisePreset : uint8_t { NoiseHat = 0, NoiseBrush, NoiseShaker, NoisePaper, NoisePresetCount };
/// Ping algorithms.
enum PingPreset : uint8_t { PingSine = 0, PingBright, PingRing, PingFeedback, PingPresetCount };

/// Everything that describes one pad. Plain data, eight 0..1 macros whose
/// meaning depends on the type (see macroName), plus common controls.
/// This is the struct a hardware front panel would edit.
struct PadParams {
    PadType type      = PadType::Click;
    uint8_t preset    = 0;
    uint8_t chokeGroup = 0;      // 0 = none
    uint8_t maxPoly   = 2;
    uint8_t flags     = 0;
    uint8_t baseNote  = 60;      // reference note when FollowsNote is set

    float macro[kNumMacros] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

    float level      = 0.8f;     // 0..1
    float pan        = 0.0f;     // -1..1
    float lowpassHz  = 20000.0f; // the "under a blanket" control
    float driveDb    = 0.0f;     // soft saturation after the lowpass
    float reverbSend = 0.0f;     // 0..1
    float tailCutMs  = 0.0f;     // 0 = off; otherwise hard cut after this long
    float reverseMs  = 120.0f;   // length of a reversed hit
    float velToLevel = 0.8f;     // 0..1, how much velocity moves level
    float velToTone  = 0.5f;     // 0..1, how much velocity brightens the hit
    float scatterMul = 1.0f;     // multiplies the global timing scatter
    float morph      = 0.0f;     // 0..1, per-hit random drift of the macros (deterministic per seed)
};

/// Human-readable macro names per type (for GUI and panel labels).
inline const char* macroName(PadType t, int i) {
    static const char* const kick[kNumMacros]  = { "Tune", "Decay", "Tone", "Pitch Env", "Attack", "Drive", "Redux", "Pitch Time" };
    static const char* const click[kNumMacros] = { "Color", "Decay", "Ring", "Bits", "Density", "Spread", "Rate", "Tone" };
    static const char* const modal[kNumMacros] = { "Tune", "Decay", "Bright", "Spread", "Hardness", "Damp", "Strike", "Stick" };
    static const char* const noise[kNumMacros] = { "Highpass", "Decay", "Band", "Metal", "Attack", "Grit", "Bits", "Tone" };
    static const char* const ping[kNumMacros]  = { "Tune", "Decay", "Bend", "Bend Time", "FM Ratio", "FM Amount", "FM Decay", "Noise" };   // Bend: above centre starts high and drops
    if (i < 0 || i >= kNumMacros) return "";
    switch (t) {
        case PadType::Kick:  return kick[i];
        case PadType::Click: return click[i];
        case PadType::Modal: return modal[i];
        case PadType::Noise: return noise[i];
        case PadType::Ping:  return ping[i];
        default: return "";
    }
}

inline const char* padTypeName(PadType t) {
    switch (t) {
        case PadType::Kick:  return "Kick";
        case PadType::Click: return "Click";
        case PadType::Modal: return "Modal";
        case PadType::Noise: return "Noise";
        case PadType::Ping:  return "Ping";
        default: return "";
    }
}

/// Number of presets for a type.
int padPresetCount(PadType t);
const char* padPresetName(PadType t, int preset);

/// Helpers for mapping a 0..1 macro to a range.
inline float mapLin(float m, float lo, float hi) { return lo + (hi - lo) * m; }
inline float mapExp(float m, float lo, float hi);  // geometric interpolation

} // namespace hic

#include <cmath>
namespace hic {
inline float mapExp(float m, float lo, float hi) { return lo * std::exp(m * std::log(hi / lo)); }
}
