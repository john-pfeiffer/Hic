#pragma once
#include <atomic>
#include <cstring>
#include <juce_audio_processors/juce_audio_processors.h>
#include "hic/Engine.h"
#include "Params.h"

namespace hicplug {

/// Moves parameters and patterns from the message thread to the engine
/// without locks: parameters are atomics read once per block, patterns are
/// double-buffered and copied when their version changes.
class EngineBridge {
public:
    explicit EngineBridge(juce::AudioProcessorValueTreeState& s) : state(s) {
        cache(id::kit, kitSel); cache(id::out, out); cache(id::seed, seed); cache(id::sync, sync); cache(id::play, play); cache(id::bpm, bpm);
        cache(id::lookahead, lookahead); cache(id::pattern, pattern); cache(id::seqEnable, seqEnable); cache(id::swing, swing);
        cache(id::busDrive, busDrive); cache(id::busDamp, busDamp); cache(id::busTexture, busTexture); cache(id::busSpace, busSpace);
        cache(id::busDrift, busDrift); cache(id::busFeel, busFeel); cache(id::keyRoot, keyRoot); cache(id::keyScale, keyScale);
        cache(id::nudge, nudge); cache(id::scatter, scatter); cache(id::velScatter, velScatter);
        for (int i = 0; i < hic::kNumPads; ++i)
            for (int p = 0; p < id::PadParamCount; ++p) padRaw[i][p] = state.getRawParameterValue(id::pad(i, static_cast<id::PadParam>(p)));
        cache(id::stLevel, stLevel); cache(id::stDensity, stDensity); cache(id::stColour, stColour); cache(id::stWarmth, stWarmth);
        cache(id::stClock, stClock); cache(id::stPulse, stPulse); cache(id::stAtt, stAtt); cache(id::stRel, stRel);
        cache(id::duckDepth, duckDepth); cache(id::duckHold, duckHold); cache(id::duckRel, duckRel);
        cache(id::repOn, repOn); cache(id::repGrid, repGrid); cache(id::repProb, repProb); cache(id::repLen, repLen);
        cache(id::repMinBars, repMinBars); cache(id::repMix, repMix);
        cache(id::frzHold, frzHold); cache(id::frzGrain, frzGrain); cache(id::frzDensity, frzDensity); cache(id::frzSpray, frzSpray);
        cache(id::frzJitter, frzJitter); cache(id::frzMix, frzMix);
        cache(id::revType, revType); cache(id::revDecay, revDecay); cache(id::revDamp, revDamp); cache(id::revPre, revPre);

        for (int k = 0; k < hic::KitCount; ++k) hic::makeKit(static_cast<hic::KitId>(k), kits[k]);
        for (auto& p : editPatterns) hic::clearPattern(p);
        hic::makeDemoPattern(editPatterns[0]);
        publishPatterns();
    }

    // ---- message thread ----
    hic::Pattern& editPattern(int i) { return editPatterns[juce::jlimit(0, hic::kNumPatterns - 1, i)]; }
    const hic::Pattern* allEditPatterns() const { return editPatterns; }

    void publishPatterns() {
        const int slot = 1 - activeSlot.load();
        std::memcpy(shared[slot], editPatterns, sizeof(editPatterns));
        activeSlot.store(slot);
        version.fetch_add(1);
    }

    /// Writes a factory kit (pads and bus) into the parameters.
    void loadKit(int kitIndex) {
        kitIndex = juce::jlimit(0, hic::KitCount - 1, kitIndex);
        const hic::KitParams& k = kits[kitIndex];
        auto set = [&](const juce::String& pid, float v) {
            if (auto* p = state.getParameter(pid)) p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        set(id::kit, static_cast<float>(kitIndex));
        for (int i = 0; i < hic::kNumPads; ++i) {
            const hic::PadParams& p = k.pads[i];
            set(id::pad(i, id::Tune), hic::tuneToHz(p.macro[hic::MacroTune]));
            set(id::pad(i, id::Decay), hic::decayToMs(p.macro[hic::MacroDecay]));
            set(id::pad(i, id::Exciter), p.macro[hic::MacroExciter]);
            set(id::pad(i, id::Body), p.macro[hic::MacroBody]);
            set(id::pad(i, id::Break), p.macro[hic::MacroBreak]);
            set(id::pad(i, id::Drift), p.macro[hic::MacroDrift]);
            set(id::pad(i, id::Level), p.level); set(id::pad(i, id::Pan), p.pan); set(id::pad(i, id::Send), p.reverbSend);
            set(id::pad(i, id::Choke), p.chokeGroup); set(id::pad(i, id::Poly), p.maxPoly);
            set(id::pad(i, id::Reverse), (p.flags & hic::PadReverse) ? 1.0f : 0.0f);
            set(id::pad(i, id::FreezeSrc), (p.flags & hic::PadFreezeSource) ? 1.0f : 0.0f);
            set(id::pad(i, id::DuckSrc), (p.flags & hic::PadDuckSource) ? 1.0f : 0.0f);
            set(id::pad(i, id::ScatterMul), p.scatterMul);
            set(id::pad(i, id::FollowKey), (p.flags & hic::PadFollowKey) ? 1.0f : 0.0f);
        }
        set(id::busDrive, k.bus.drive); set(id::busDamp, k.bus.damp); set(id::busTexture, k.bus.texture);
        set(id::busSpace, k.bus.space); set(id::busDrift, k.bus.drift); set(id::busFeel, k.bus.feel);
    }

    void loadPatterns(const void* data, size_t bytes) {
        if (bytes == sizeof(editPatterns)) { std::memcpy(editPatterns, data, bytes); publishPatterns(); }
    }

    // ---- audio thread ----
    void apply(hic::Engine& e) {
        e.global.outputDb = *out;
        e.global.internalPlay = *sync > 0.5f && *play > 0.5f;
        e.global.internalBpm = static_cast<double>(*bpm);
        e.global.seqEnabled = *seqEnable > 0.5f;
        e.global.activePattern = static_cast<int>(*pattern) - 1;
        e.feel.seed = static_cast<uint32_t>(*seed);
        e.feel.lookaheadMs = *lookahead;
        e.feel.nudgeMs = *nudge;
        e.feel.scatterMs = *scatter;
        e.feel.velScatter = *velScatter;

        e.bus.drive = *busDrive; e.bus.damp = *busDamp; e.bus.texture = *busTexture;
        e.bus.space = *busSpace; e.bus.drift = *busDrift; e.bus.feel = *busFeel;
        e.key.root = static_cast<uint8_t>(juce::jlimit(0, 11, static_cast<int>(*keyRoot + 0.5f)));
        e.key.scale = static_cast<uint8_t>(juce::jlimit(0, hic::ScaleCount - 1, static_cast<int>(*keyScale + 0.5f)));

        // Note tracking, base notes and the note map come from the selected factory kit.
        const hic::KitParams& base = kits[juce::jlimit(0, hic::KitCount - 1, static_cast<int>(*kitSel + 0.5f))];
        std::memcpy(e.kit.noteToPad, base.noteToPad, sizeof(e.kit.noteToPad));
        e.kit.seed = base.seed;
        for (int i = 0; i < hic::kNumPads; ++i) {
            hic::PadParams& p = e.kit.pads[i];
            const auto& r = padRaw[i];
            p.baseNote = base.pads[i].baseNote;
            p.reverseMs = base.pads[i].reverseMs;
            p.velToLevel = base.pads[i].velToLevel;
            p.velToTone = base.pads[i].velToTone;
            p.macro[hic::MacroTune]    = hic::hzToTune(juce::jlimit(hic::kTuneLoHz, hic::kTuneHiHz, r[id::Tune]->load()));
            p.macro[hic::MacroDecay]   = hic::msToDecay(juce::jlimit(hic::kDecayLoMs, hic::kDecayHiMs, r[id::Decay]->load()));
            p.macro[hic::MacroExciter] = *r[id::Exciter];
            p.macro[hic::MacroBody]    = *r[id::Body];
            p.macro[hic::MacroBreak]   = *r[id::Break];
            p.macro[hic::MacroDrift]   = *r[id::Drift];
            p.level = *r[id::Level]; p.pan = *r[id::Pan]; p.reverbSend = *r[id::Send];
            p.chokeGroup = static_cast<uint8_t>(*r[id::Choke] + 0.5f);
            p.maxPoly = static_cast<uint8_t>(juce::jlimit(1.0f, 8.0f, *r[id::Poly] + 0.5f));
            p.scatterMul = *r[id::ScatterMul];
            uint8_t flags = base.pads[i].flags & hic::PadFollowsNote;
            if (*r[id::Reverse] > 0.5f) flags |= hic::PadReverse;
            if (*r[id::FreezeSrc] > 0.5f) flags |= hic::PadFreezeSource;
            if (*r[id::DuckSrc] > 0.5f) flags |= hic::PadDuckSource;
            if (*r[id::FollowKey] > 0.5f) flags |= hic::PadFollowKey;
            p.flags = flags;
        }

        e.statik.levelDetail = *stLevel; e.statik.density = *stDensity; e.statik.colour = *stColour; e.statik.warmth = *stWarmth;
        e.statik.clock = static_cast<hic::StaticClock>(juce::jlimit(0, 4, static_cast<int>(*stClock + 0.5f)));
        e.statik.pulseMs = *stPulse; e.statik.attackMs = *stAtt; e.statik.releaseMs = *stRel;
        e.duck.depthDb = *duckDepth; e.duck.holdMs = *duckHold; e.duck.releaseMs = *duckRel;

        e.repeat.enabled = *repOn > 0.5f;
        static const int grids[3] = { 16, 32, 64 };
        e.repeat.grid = grids[juce::jlimit(0, 2, static_cast<int>(*repGrid + 0.5f))];
        e.repeat.probability = *repProb;
        static const float lens[5] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f };
        e.repeat.lengthBeats = lens[juce::jlimit(0, 4, static_cast<int>(*repLen + 0.5f))];
        e.repeat.minBarsBetween = static_cast<int>(*repMinBars + 0.5f);
        e.repeat.mix = *repMix;

        e.freeze.hold = *frzHold > 0.5f; e.freeze.grainMs = *frzGrain; e.freeze.density = *frzDensity;
        e.freeze.sprayMs = *frzSpray; e.freeze.pitchJitterCents = *frzJitter; e.freeze.mix = *frzMix;

        e.reverb.type = *revType > 0.5f ? hic::ReverbType::Room : hic::ReverbType::Spring;
        e.reverb.decaySec = *revDecay; e.reverb.dampHz = *revDamp; e.reverb.predelayMs = *revPre;

        // Patterns: copy when a new version was published; retry if it changed mid-copy.
        for (int attempt = 0; attempt < 4; ++attempt) {
            const int v = version.load();
            if (v == e.patternVersion) break;
            const int slot = activeSlot.load();
            std::memcpy(e.patterns, shared[slot], sizeof(e.patterns));
            if (version.load() == v) { e.patternVersion = v; break; }
        }
        for (auto& p : e.patterns) p.swingPct = static_cast<uint8_t>(*swing + 0.5f);
    }

private:
    void cache(const juce::String& idStr, std::atomic<float>*& dst) { dst = state.getRawParameterValue(idStr); jassert(dst != nullptr); }

    juce::AudioProcessorValueTreeState& state;
    std::atomic<float> *kitSel, *out, *seed, *sync, *play, *bpm, *lookahead, *pattern, *seqEnable, *swing;
    std::atomic<float> *busDrive, *busDamp, *busTexture, *busSpace, *busDrift, *busFeel, *keyRoot, *keyScale;
    std::atomic<float> *nudge, *scatter, *velScatter;
    std::atomic<float>* padRaw[hic::kNumPads][id::PadParamCount];
    std::atomic<float> *stLevel, *stDensity, *stColour, *stWarmth, *stClock, *stPulse, *stAtt, *stRel;
    std::atomic<float> *duckDepth, *duckHold, *duckRel;
    std::atomic<float> *repOn, *repGrid, *repProb, *repLen, *repMinBars, *repMix;
    std::atomic<float> *frzHold, *frzGrain, *frzDensity, *frzSpray, *frzJitter, *frzMix;
    std::atomic<float> *revType, *revDecay, *revDamp, *revPre;

    hic::KitParams kits[hic::KitCount];
    hic::Pattern editPatterns[hic::kNumPatterns];
    hic::Pattern shared[2][hic::kNumPatterns];
    std::atomic<int> activeSlot { 0 };
    std::atomic<int> version { 0 };
};

} // namespace hicplug
