#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "hic/Config.h"
#include "hic/Pad.h"

namespace hicplug {

/// Parameter IDs. Pad parameters are "p<index>_<name>".
namespace id {
    inline const juce::String kit        = "kit";         // 0 Neon, 1 Micro
    inline const juce::String out        = "out";
    inline const juce::String seed       = "seed";
    inline const juce::String sync       = "sync";        // 0 host, 1 internal
    inline const juce::String play       = "play";
    inline const juce::String bpm        = "bpm";
    inline const juce::String lookahead  = "lookahead";
    inline const juce::String pattern    = "pattern";
    inline const juce::String seqEnable  = "seq_enable";
    inline const juce::String swing      = "swing";
    inline const juce::String nudge      = "feel_nudge";
    inline const juce::String scatter    = "feel_scatter";
    inline const juce::String velScatter = "feel_vel";

    inline const juce::String bedType    = "bed_type";
    inline const juce::String bedLevel   = "bed_level";
    inline const juce::String bedDensity = "bed_density";
    inline const juce::String bedWarmth  = "bed_warmth";
    inline const juce::String bedPop     = "bed_pop";
    inline const juce::String bedColor   = "bed_color";
    inline const juce::String bedHiss    = "bed_hiss";
    inline const juce::String bedGate    = "bed_gate";
    inline const juce::String bedGateAtt = "bed_gate_att";
    inline const juce::String bedGateRel = "bed_gate_rel";
    inline const juce::String bedGateDuty= "bed_gate_duty";
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
    inline const juce::String revMix     = "rev_mix";

    /// Per-pad parameter names (suffixes).
    enum PadParam { Type = 0, Preset, M0, M1, M2, M3, M4, M5, M6, M7, Level, Pan, Lowpass, Drive, Send, TailCut,
                    Reverse, Choke, Poly, FreezeSrc, DuckSrc, ScatterMul, Morph, PadParamCount };
    const char* padSuffix(PadParam p);
    juce::String pad(int index, PadParam p);
}

/// Builds the full parameter layout with defaults taken from the default kit.
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/// Human-readable preset names for the pad type combo boxes.
juce::StringArray presetNames(hic::PadType type);
juce::StringArray padTypeNames();
juce::StringArray kitNames();

} // namespace hicplug
