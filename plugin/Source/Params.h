#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "hic/Config.h"
#include "hic/Pad.h"

namespace hicplug {

/// Parameter IDs (v2: one voice, six macros per pad, six bus macros).
namespace id {
    inline const juce::String kit        = "kit";         // 0 Neon, 1 Micro, 2 Modular
    inline const juce::String out        = "out";
    inline const juce::String seed       = "seed";
    inline const juce::String sync       = "sync";        // 0 host, 1 internal
    inline const juce::String play       = "play";
    inline const juce::String bpm        = "bpm";
    inline const juce::String lookahead  = "lookahead";
    inline const juce::String pattern    = "pattern";
    inline const juce::String seqEnable  = "seq_enable";
    inline const juce::String swing      = "swing";

    inline const juce::String busDrive   = "bus_drive";
    inline const juce::String busDamp    = "bus_damp";
    inline const juce::String busTexture = "bus_texture";
    inline const juce::String busSpace   = "bus_space";
    inline const juce::String busDrift   = "bus_drift";
    inline const juce::String busFeel    = "bus_feel";
    inline const juce::String keyRoot    = "key_root";
    inline const juce::String keyScale   = "key_scale";

    inline const juce::String nudge      = "feel_nudge";
    inline const juce::String scatter    = "feel_scatter";   // maximum, scaled by bus Feel
    inline const juce::String velScatter = "feel_vel";       // maximum, scaled by bus Feel

    inline const juce::String stLevel    = "st_level";
    inline const juce::String stDensity  = "st_density";
    inline const juce::String stColour   = "st_colour";
    inline const juce::String stWarmth   = "st_warmth";
    inline const juce::String stClock    = "st_clock";
    inline const juce::String stPulse    = "st_pulse";
    inline const juce::String stAtt      = "st_att";
    inline const juce::String stRel      = "st_rel";
    inline const juce::String duckDepth  = "duck_depth";
    inline const juce::String duckHold   = "duck_hold";
    inline const juce::String duckRel    = "duck_release";

    inline const juce::String repOn      = "rep_on";
    inline const juce::String repGrid    = "rep_grid";
    inline const juce::String repProb    = "rep_prob";
    inline const juce::String repLen     = "rep_len";
    inline const juce::String repMinBars = "rep_min_bars";
    inline const juce::String repMix     = "rep_mix";

    inline const juce::String frzHold    = "frz_hold";
    inline const juce::String frzGrain   = "frz_grain";
    inline const juce::String frzDensity = "frz_density";
    inline const juce::String frzSpray   = "frz_spray";
    inline const juce::String frzJitter  = "frz_jitter";
    inline const juce::String frzMix     = "frz_mix";

    inline const juce::String revType    = "rev_type";
    inline const juce::String revDecay   = "rev_decay";
    inline const juce::String revDamp    = "rev_damp";
    inline const juce::String revPre     = "rev_predelay";

    /// Per-pad parameter names (suffixes).
    enum PadParam { Tune = 0, Decay, Exciter, Body, Break, Drift, Level, Pan, Send, Choke, Poly, Reverse,
                    FreezeSrc, DuckSrc, ScatterMul, FollowKey, PadParamCount };
    const char* padSuffix(PadParam p);
    juce::String pad(int index, PadParam p);
}

/// Builds the full parameter layout with defaults taken from the Neon kit.
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

juce::StringArray kitNames();
juce::StringArray keyRootNames();
juce::StringArray keyScaleNames();

} // namespace hicplug
