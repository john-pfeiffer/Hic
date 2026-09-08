#include "Params.h"
#include "hic/Kit.h"
#include "hic/voices/ModalPresets.h"
#include "hic/Engine.h"

using namespace juce;

namespace hicplug {

const char* id::padSuffix(PadParam p) {
    static const char* const names[PadParamCount] = {
        "type", "preset", "m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7", "level", "pan", "lowpass", "drive",
        "send", "tailcut", "reverse", "choke", "poly", "freeze_src", "duck_src", "scatter_mul"
    };
    return names[p];
}

String id::pad(int index, PadParam p) { return "p" + String(index) + "_" + padSuffix(p); }

StringArray padTypeNames() { return { "Kick", "Click", "Modal", "Noise" }; }

StringArray presetNames(hic::PadType type) {
    StringArray out;
    for (int i = 0; i < hic::padPresetCount(type); ++i) out.add(hic::padPresetName(type, i));
    return out;
}

static std::unique_ptr<AudioParameterFloat> lin(const String& idStr, const String& name, float lo, float hi, float def, const String& unit = {}) {
    return std::make_unique<AudioParameterFloat>(ParameterID{ idStr, 1 }, name, NormalisableRange<float>(lo, hi), def,
        AudioParameterFloatAttributes().withLabel(unit));
}
static std::unique_ptr<AudioParameterFloat> log(const String& idStr, const String& name, float lo, float hi, float def, const String& unit = {}) {
    NormalisableRange<float> range(lo, hi, 0.0f, 0.3f);
    return std::make_unique<AudioParameterFloat>(ParameterID{ idStr, 1 }, name, range, def,
        AudioParameterFloatAttributes().withLabel(unit));
}

AudioProcessorValueTreeState::ParameterLayout createLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;
    hic::KitParams kit; hic::makeDefaultKit(kit);

    // Global and feel
    layout.add(lin(id::out, "Output", -60.0f, 12.0f, 0.0f, "dB"));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::seed, 1 }, "Seed", 1, 999, 1));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::sync, 1 }, "Sync", StringArray{ "Host", "Internal" }, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::play, 1 }, "Play", false));
    layout.add(lin(id::bpm, "BPM", 40.0f, 240.0f, 92.0f));
    layout.add(lin(id::lookahead, "Lookahead", 0.0f, 40.0f, 20.0f, "ms"));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pattern, 1 }, "Pattern", 1, hic::kNumPatterns, 1));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::seqEnable, 1 }, "Sequencer", true));
    layout.add(lin(id::swing, "Swing", 50.0f, 75.0f, 54.0f, "%"));
    layout.add(lin(id::nudge, "Nudge", -20.0f, 20.0f, 0.0f, "ms"));
    layout.add(lin(id::scatter, "Scatter", 0.0f, 20.0f, 3.0f, "ms"));
    layout.add(lin(id::velScatter, "Vel Scatter", 0.0f, 0.5f, 0.08f));

    // Pads
    for (int i = 0; i < hic::kNumPads; ++i) {
        const hic::PadParams& p = kit.pads[i];
        const String base = hic::defaultPadName(i) + String(" ");
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::pad(i, id::Type), 1 }, base + "Type", padTypeNames(), static_cast<int>(p.type)));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pad(i, id::Preset), 1 }, base + "Preset", 0, hic::ModalPresetCount - 1, p.preset));
        for (int m = 0; m < hic::kNumMacros; ++m)
            layout.add(lin(id::pad(i, static_cast<id::PadParam>(id::M0 + m)), base + "Macro " + String(m + 1), 0.0f, 1.0f, p.macro[m]));
        layout.add(lin(id::pad(i, id::Level), base + "Level", 0.0f, 1.0f, p.level));
        layout.add(lin(id::pad(i, id::Pan), base + "Pan", -1.0f, 1.0f, p.pan));
        layout.add(log(id::pad(i, id::Lowpass), base + "Lowpass", 200.0f, 20000.0f, p.lowpassHz, "Hz"));
        layout.add(lin(id::pad(i, id::Drive), base + "Drive", 0.0f, 18.0f, p.driveDb, "dB"));
        layout.add(lin(id::pad(i, id::Send), base + "Reverb Send", 0.0f, 1.0f, p.reverbSend));
        layout.add(lin(id::pad(i, id::TailCut), base + "Tail Cut", 0.0f, 500.0f, p.tailCutMs, "ms"));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::Reverse), 1 }, base + "Reverse", (p.flags & hic::PadReverse) != 0));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pad(i, id::Choke), 1 }, base + "Choke Group", 0, 4, p.chokeGroup));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pad(i, id::Poly), 1 }, base + "Polyphony", 1, 8, p.maxPoly));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::FreezeSrc), 1 }, base + "Freeze Source", (p.flags & hic::PadFreezeSource) != 0));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::DuckSrc), 1 }, base + "Duck Source", (p.flags & hic::PadDuckSource) != 0));
        layout.add(lin(id::pad(i, id::ScatterMul), base + "Scatter Amount", 0.0f, 3.0f, p.scatterMul));
    }

    // Bed and ducker
    hic::BedParams bed; hic::DuckParams duck;
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::bedType, 1 }, "Bed", StringArray{ "Off", "Crackle", "Hiss", "Both" }, static_cast<int>(bed.type)));
    layout.add(lin(id::bedLevel, "Bed Level", 0.0f, 1.0f, bed.level));
    layout.add(log(id::bedDensity, "Crackle Density", 5.0f, 200.0f, bed.density, "/s"));
    layout.add(log(id::bedWarmth, "Crackle Warmth", 2000.0f, 10000.0f, bed.warmth, "Hz"));
    layout.add(lin(id::bedPop, "Pops", 0.0f, 0.3f, bed.popRatio));
    layout.add(lin(id::bedColor, "Hiss Color", 0.0f, 1.0f, bed.color));
    layout.add(lin(id::bedHiss, "Hiss Level", 0.0f, 1.0f, bed.hissLevel));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::bedGate, 1 }, "Bed Gate", StringArray{ "Off", "16ths", "8ths", "Quarters", "Steps" }, static_cast<int>(bed.gate)));
    layout.add(lin(id::bedGateAtt, "Gate Attack", 0.5f, 50.0f, bed.gateAttackMs, "ms"));
    layout.add(lin(id::bedGateRel, "Gate Release", 5.0f, 400.0f, bed.gateReleaseMs, "ms"));
    layout.add(lin(id::bedGateDuty, "Gate Open", 0.1f, 0.9f, bed.gateDuty));
    layout.add(lin(id::duckDepth, "Duck Depth", -40.0f, 0.0f, duck.depthDb, "dB"));
    layout.add(lin(id::duckHold, "Duck Hold", 0.0f, 200.0f, duck.holdMs, "ms"));
    layout.add(lin(id::duckRel, "Duck Release", 20.0f, 600.0f, duck.releaseMs, "ms"));

    // Beat repeat
    hic::RepeatParams rep;
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::repOn, 1 }, "Repeat", rep.enabled));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::repGrid, 1 }, "Repeat Grid", StringArray{ "1/16", "1/32", "1/64" }, 1));
    layout.add(lin(id::repProb, "Repeat Chance", 0.0f, 1.0f, rep.probability));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::repLen, 1 }, "Repeat Length", StringArray{ "1/8", "1/4", "1/2", "1 beat", "2 beats" }, 2));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::repMinBars, 1 }, "Bars Between", 1, 8, rep.minBarsBetween));
    layout.add(lin(id::repMix, "Repeat Mix", 0.0f, 1.0f, rep.mix));

    // Freeze
    hic::FreezeParams frz;
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::frzHold, 1 }, "Freeze Hold", false));
    layout.add(lin(id::frzGrain, "Grain", 4.0f, 40.0f, frz.grainMs, "ms"));
    layout.add(log(id::frzDensity, "Grain Density", 5.0f, 200.0f, frz.density, "/s"));
    layout.add(lin(id::frzSpray, "Spray", 0.0f, 80.0f, frz.sprayMs, "ms"));
    layout.add(lin(id::frzJitter, "Pitch Jitter", 0.0f, 100.0f, frz.pitchJitterCents, "ct"));
    layout.add(lin(id::frzMix, "Freeze Mix", 0.0f, 1.0f, frz.mix));

    // Reverb
    hic::ReverbParams rev;
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::revType, 1 }, "Reverb", StringArray{ "Spring", "Room" }, 0));
    layout.add(lin(id::revDecay, "Reverb Decay", 0.2f, 1.2f, rev.decaySec, "s"));
    layout.add(log(id::revDamp, "Reverb Damp", 2000.0f, 6000.0f, rev.dampHz, "Hz"));
    layout.add(lin(id::revPre, "Predelay", 5.0f, 20.0f, rev.predelayMs, "ms"));
    layout.add(lin(id::revMix, "Reverb Return", 0.0f, 1.0f, rev.mix));

    return layout;
}

} // namespace hicplug
