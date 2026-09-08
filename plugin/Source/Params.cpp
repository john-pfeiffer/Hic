#include "Params.h"
#include "hic/Kit.h"
#include "hic/Key.h"
#include "hic/Engine.h"

using namespace juce;

namespace hicplug {

const char* id::padSuffix(PadParam p) {
    static const char* const names[PadParamCount] = {
        "tune", "decay", "exciter", "body", "break", "drift", "level", "pan", "send", "choke", "poly", "reverse",
        "freeze_src", "duck_src", "scatter_mul", "follow_key"
    };
    return names[p];
}

String id::pad(int index, PadParam p) { return "p" + String(index) + "_" + padSuffix(p); }

StringArray kitNames() { return { "Neon", "Micro", "Modular" }; }
StringArray keyRootNames() { StringArray a; for (int i = 0; i < 12; ++i) a.add(hic::noteName(i)); return a; }
StringArray keyScaleNames() { StringArray a; for (int i = 0; i < hic::ScaleCount; ++i) a.add(hic::keyScaleName(i)); return a; }

static AudioParameterFloatAttributes attrs(const String& unit, float hi) {
    const int decimals = hi > 500.0f ? 0 : (hi > 30.0f ? 1 : 2);
    return AudioParameterFloatAttributes().withLabel(unit)
        .withStringFromValueFunction([decimals](float v, int) { return String(v, decimals); });
}
static std::unique_ptr<AudioParameterFloat> lin(const String& idStr, const String& name, float lo, float hi, float def, const String& unit = {}) {
    return std::make_unique<AudioParameterFloat>(ParameterID{ idStr, 2 }, name, NormalisableRange<float>(lo, hi), def, attrs(unit, hi));
}
static std::unique_ptr<AudioParameterFloat> log(const String& idStr, const String& name, float lo, float hi, float def, const String& unit = {}) {
    NormalisableRange<float> range(lo, hi, 0.0f, 0.3f);
    return std::make_unique<AudioParameterFloat>(ParameterID{ idStr, 2 }, name, range, def, attrs(unit, hi));
}
static String tuneText(float hz, int) {
    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    const int n = int(std::floor(midi + 0.5f));
    return String(hz, hz < 100.0f ? 1 : 0) + " Hz \xc2\xb7 " + hic::noteName(n) + String(n / 12 - 1);
}

AudioProcessorValueTreeState::ParameterLayout createLayout() {
    AudioProcessorValueTreeState::ParameterLayout layout;
    hic::KitParams kit; hic::makeDefaultKit(kit);

    // Global and transport
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::kit, 2 }, "Kit", kitNames(), 0));
    layout.add(lin(id::out, "Output", -60.0f, 12.0f, 0.0f, "dB"));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::seed, 2 }, "Seed", 1, 999, 1));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::sync, 2 }, "Sync", StringArray{ "Host", "Internal" }, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::play, 2 }, "Play", false));
    layout.add(lin(id::bpm, "BPM", 40.0f, 240.0f, 92.0f));
    layout.add(lin(id::lookahead, "Lookahead", 0.0f, 40.0f, 20.0f, "ms"));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pattern, 2 }, "Pattern", 1, hic::kNumPatterns, 1));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::seqEnable, 2 }, "Sequencer", true));
    layout.add(lin(id::swing, "Swing", 50.0f, 75.0f, 54.0f, "%"));

    // Bus and key
    hic::BusParams bus = kit.bus;
    layout.add(lin(id::busDrive, "Drive", 0.0f, 1.0f, bus.drive));
    layout.add(lin(id::busDamp, "Damp", 0.0f, 1.0f, bus.damp));
    layout.add(lin(id::busTexture, "Texture", 0.0f, 1.0f, bus.texture));
    layout.add(lin(id::busSpace, "Space", 0.0f, 1.0f, bus.space));
    layout.add(lin(id::busDrift, "Drift", 0.0f, 2.0f, bus.drift));
    layout.add(lin(id::busFeel, "Feel", 0.0f, 1.0f, bus.feel));
    hic::KeyParams key;
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::keyRoot, 2 }, "Key Root", keyRootNames(), key.root));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::keyScale, 2 }, "Key Scale", keyScaleNames(), key.scale));

    // Feel detail
    hic::FeelParams feel;
    layout.add(lin(id::nudge, "Nudge", -20.0f, 20.0f, feel.nudgeMs, "ms"));
    layout.add(lin(id::scatter, "Scatter Max", 0.0f, 20.0f, feel.scatterMs, "ms"));
    layout.add(lin(id::velScatter, "Vel Scatter Max", 0.0f, 0.5f, feel.velScatter));

    // Pads
    for (int i = 0; i < hic::kNumPads; ++i) {
        const hic::PadParams& p = kit.pads[i];
        const String base = hic::defaultPadName(i) + String(" ");
        NormalisableRange<float> tuneRange(hic::kTuneLoHz, hic::kTuneHiHz, 0.0f, 0.25f);
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID{ id::pad(i, id::Tune), 2 }, base + "Tune", tuneRange, hic::tuneToHz(p.macro[hic::MacroTune]),
            AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction(tuneText)));
        layout.add(log(id::pad(i, id::Decay), base + "Decay", hic::kDecayLoMs, hic::kDecayHiMs, hic::decayToMs(p.macro[hic::MacroDecay]), "ms"));
        layout.add(lin(id::pad(i, id::Exciter), base + "Exciter", 0.0f, 1.0f, p.macro[hic::MacroExciter]));
        layout.add(lin(id::pad(i, id::Body), base + "Body", 0.0f, 1.0f, p.macro[hic::MacroBody]));
        layout.add(lin(id::pad(i, id::Break), base + "Break", 0.0f, 1.0f, p.macro[hic::MacroBreak]));
        layout.add(lin(id::pad(i, id::Drift), base + "Drift", 0.0f, 1.0f, p.macro[hic::MacroDrift]));
        layout.add(lin(id::pad(i, id::Level), base + "Level", 0.0f, 1.0f, p.level));
        layout.add(lin(id::pad(i, id::Pan), base + "Pan", -1.0f, 1.0f, p.pan));
        layout.add(lin(id::pad(i, id::Send), base + "Reverb Send", 0.0f, 1.0f, p.reverbSend));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pad(i, id::Choke), 2 }, base + "Choke Group", 0, 4, p.chokeGroup));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::pad(i, id::Poly), 2 }, base + "Polyphony", 1, 8, p.maxPoly));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::Reverse), 2 }, base + "Reverse", (p.flags & hic::PadReverse) != 0));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::FreezeSrc), 2 }, base + "Freeze Source", (p.flags & hic::PadFreezeSource) != 0));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::DuckSrc), 2 }, base + "Duck Source", (p.flags & hic::PadDuckSource) != 0));
        layout.add(lin(id::pad(i, id::ScatterMul), base + "Scatter Amount", 0.0f, 3.0f, p.scatterMul));
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::pad(i, id::FollowKey), 2 }, base + "Follow Key", (p.flags & hic::PadFollowKey) != 0));
    }

    // Static and ducker detail
    hic::StaticParams st; hic::DuckParams duck;
    layout.add(lin(id::stLevel, "Static Level", 0.0f, 1.0f, st.levelDetail));
    layout.add(log(id::stDensity, "Static Density", 5.0f, 400.0f, st.density, "/s"));
    layout.add(lin(id::stColour, "Static Colour", 0.0f, 1.0f, st.colour));
    layout.add(log(id::stWarmth, "Static Warmth", 2000.0f, 12000.0f, st.warmth, "Hz"));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::stClock, 2 }, "Static Clock", StringArray{ "Off", "16ths", "8ths", "Quarters", "Steps" }, static_cast<int>(st.clock)));
    layout.add(log(id::stPulse, "Static Pulse", 5.0f, 500.0f, st.pulseMs, "ms"));
    layout.add(lin(id::stAtt, "Static Attack", 0.5f, 50.0f, st.attackMs, "ms"));
    layout.add(lin(id::stRel, "Static Release", 5.0f, 400.0f, st.releaseMs, "ms"));
    layout.add(lin(id::duckDepth, "Duck Depth", -40.0f, 0.0f, duck.depthDb, "dB"));
    layout.add(lin(id::duckHold, "Duck Hold", 0.0f, 200.0f, duck.holdMs, "ms"));
    layout.add(lin(id::duckRel, "Duck Release", 20.0f, 600.0f, duck.releaseMs, "ms"));

    // Beat repeat
    hic::RepeatParams rep;
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::repOn, 2 }, "Repeat", rep.enabled));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::repGrid, 2 }, "Repeat Grid", StringArray{ "1/16", "1/32", "1/64" }, 1));
    layout.add(lin(id::repProb, "Repeat Chance", 0.0f, 1.0f, rep.probability));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::repLen, 2 }, "Repeat Length", StringArray{ "1/8", "1/4", "1/2", "1 beat", "2 beats" }, 2));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID{ id::repMinBars, 2 }, "Bars Between", 1, 8, rep.minBarsBetween));
    layout.add(lin(id::repMix, "Repeat Mix", 0.0f, 1.0f, rep.mix));

    // Freeze
    hic::FreezeParams frz;
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{ id::frzHold, 2 }, "Freeze Hold", false));
    layout.add(lin(id::frzGrain, "Grain", 4.0f, 40.0f, frz.grainMs, "ms"));
    layout.add(log(id::frzDensity, "Grain Density", 5.0f, 200.0f, frz.density, "/s"));
    layout.add(lin(id::frzSpray, "Spray", 0.0f, 80.0f, frz.sprayMs, "ms"));
    layout.add(lin(id::frzJitter, "Pitch Jitter", 0.0f, 100.0f, frz.pitchJitterCents, "ct"));
    layout.add(lin(id::frzMix, "Freeze Mix", 0.0f, 1.0f, frz.mix));

    // Reverb detail (amount and size come from the bus Space knob)
    hic::ReverbParams rev;
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{ id::revType, 2 }, "Reverb", StringArray{ "Spring", "Room" }, 0));
    layout.add(lin(id::revDecay, "Reverb Decay", 0.2f, 1.2f, rev.decaySec, "s"));
    layout.add(log(id::revDamp, "Reverb Damp", 2000.0f, 6000.0f, rev.dampHz, "Hz"));
    layout.add(lin(id::revPre, "Predelay", 5.0f, 20.0f, rev.predelayMs, "ms"));

    return layout;
}

} // namespace hicplug
