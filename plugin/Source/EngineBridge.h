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
        cache(id::nudge, nudge); cache(id::scatter, scatter); cache(id::velScatter, velScatter);
        for (int i = 0; i < hic::kNumPads; ++i)
            for (int p = 0; p < id::PadParamCount; ++p) padRaw[i][p] = state.getRawParameterValue(id::pad(i, static_cast<id::PadParam>(p)));
        cache(id::bedType, bedType); cache(id::bedLevel, bedLevel); cache(id::bedDensity, bedDensity); cache(id::bedWarmth, bedWarmth);
        cache(id::bedPop, bedPop); cache(id::bedColor, bedColor); cache(id::bedHiss, bedHiss); cache(id::bedGate, bedGate);
        cache(id::bedGateAtt, bedGateAtt); cache(id::bedGateRel, bedGateRel); cache(id::bedGateDuty, bedGateDuty);
        cache(id::duckDepth, duckDepth); cache(id::duckHold, duckHold); cache(id::duckRel, duckRel);
        cache(id::repOn, repOn); cache(id::repGrid, repGrid); cache(id::repProb, repProb); cache(id::repLen, repLen);
        cache(id::repMinBars, repMinBars); cache(id::repMix, repMix);
        cache(id::frzHold, frzHold); cache(id::frzGrain, frzGrain); cache(id::frzDensity, frzDensity); cache(id::frzSpray, frzSpray);
        cache(id::frzJitter, frzJitter); cache(id::frzMix, frzMix);
        cache(id::revType, revType); cache(id::revDecay, revDecay); cache(id::revDamp, revDamp); cache(id::revPre, revPre); cache(id::revMix, revMix);

        hic::makeKit(hic::KitNeon, kits[0]); hic::makeKit(hic::KitMicro, kits[1]);
        for (auto& p : editPatterns) hic::clearPattern(p);
        hic::makeDemoPattern(editPatterns[0]);
        publishPatterns();
    }

    // ---- message thread ----
    hic::Pattern& editPattern(int i) { return editPatterns[juce::jlimit(0, hic::kNumPatterns - 1, i)]; }
    const hic::Pattern* allEditPatterns() const { return editPatterns; }

    /// Copies the edit patterns into the shared slot and bumps the version.
    void publishPatterns() {
        const int slot = 1 - activeSlot.load();
        std::memcpy(shared[slot], editPatterns, sizeof(editPatterns));
        activeSlot.store(slot);
        version.fetch_add(1);
    }

    /// Writes a factory kit into the pad parameters (message thread).
    void loadKit(int kitIndex) {
        kitIndex = juce::jlimit(0, hic::KitCount - 1, kitIndex);
        const hic::KitParams& k = kits[kitIndex];
        auto set = [&](const juce::String& pid, float v) {
            if (auto* p = state.getParameter(pid)) p->setValueNotifyingHost(p->convertTo0to1(v));
        };
        set(id::kit, static_cast<float>(kitIndex));
        for (int i = 0; i < hic::kNumPads; ++i) {
            const hic::PadParams& p = k.pads[i];
            set(id::pad(i, id::Type), static_cast<float>(p.type));
            set(id::pad(i, id::Preset), p.preset);
            for (int m = 0; m < hic::kNumMacros; ++m) set(id::pad(i, static_cast<id::PadParam>(id::M0 + m)), p.macro[m]);
            set(id::pad(i, id::Level), p.level); set(id::pad(i, id::Pan), p.pan); set(id::pad(i, id::Lowpass), p.lowpassHz);
            set(id::pad(i, id::Drive), p.driveDb); set(id::pad(i, id::Send), p.reverbSend); set(id::pad(i, id::TailCut), p.tailCutMs);
            set(id::pad(i, id::Reverse), (p.flags & hic::PadReverse) ? 1.0f : 0.0f);
            set(id::pad(i, id::Choke), p.chokeGroup); set(id::pad(i, id::Poly), p.maxPoly);
            set(id::pad(i, id::FreezeSrc), (p.flags & hic::PadFreezeSource) ? 1.0f : 0.0f);
            set(id::pad(i, id::DuckSrc), (p.flags & hic::PadDuckSource) ? 1.0f : 0.0f);
            set(id::pad(i, id::ScatterMul), p.scatterMul); set(id::pad(i, id::Morph), p.morph);
        }
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

        // Note tracking, base notes and the note map come from the selected factory kit;
        // everything else is a parameter.
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
            p.type = static_cast<hic::PadType>(juce::jlimit(0, static_cast<int>(hic::PadType::Count) - 1, static_cast<int>(*r[id::Type] + 0.5f)));
            p.preset = static_cast<uint8_t>(juce::jlimit(0, hic::padPresetCount(p.type) - 1, static_cast<int>(*r[id::Preset] + 0.5f)));
            for (int m = 0; m < hic::kNumMacros; ++m) p.macro[m] = *r[id::M0 + m];
            p.level = *r[id::Level]; p.pan = *r[id::Pan]; p.lowpassHz = *r[id::Lowpass]; p.driveDb = *r[id::Drive];
            p.reverbSend = *r[id::Send]; p.tailCutMs = *r[id::TailCut];
            p.chokeGroup = static_cast<uint8_t>(*r[id::Choke] + 0.5f);
            p.maxPoly = static_cast<uint8_t>(juce::jlimit(1.0f, 8.0f, *r[id::Poly] + 0.5f));
            p.scatterMul = *r[id::ScatterMul];
            p.morph = *r[id::Morph];
            uint8_t flags = base.pads[i].flags & hic::PadFollowsNote;
            if (*r[id::Reverse] > 0.5f) flags |= hic::PadReverse;
            if (*r[id::FreezeSrc] > 0.5f) flags |= hic::PadFreezeSource;
            if (*r[id::DuckSrc] > 0.5f) flags |= hic::PadDuckSource;
            p.flags = flags;
        }

        e.bed.type = static_cast<hic::BedType>(static_cast<int>(*bedType + 0.5f));
        e.bed.level = *bedLevel; e.bed.density = *bedDensity; e.bed.warmth = *bedWarmth; e.bed.popRatio = *bedPop;
        e.bed.color = *bedColor; e.bed.hissLevel = *bedHiss;
        e.bed.gate = static_cast<hic::BedGate>(static_cast<int>(*bedGate + 0.5f));
        e.bed.gateAttackMs = *bedGateAtt; e.bed.gateReleaseMs = *bedGateRel; e.bed.gateDuty = *bedGateDuty;
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
        e.reverb.decaySec = *revDecay; e.reverb.dampHz = *revDamp; e.reverb.predelayMs = *revPre; e.reverb.mix = *revMix;

        // Patterns: copy when a new version was published; retry if it changed mid-copy.
        // The exporter uses its own Engine, so the applied version is tracked per engine.
        if (&e != appliedEngine) { appliedEngine = &e; appliedVersion = -1; }
        for (int attempt = 0; attempt < 4; ++attempt) {
            const int v = version.load();
            if (v == appliedVersion) break;
            const int slot = activeSlot.load();
            std::memcpy(e.patterns, shared[slot], sizeof(e.patterns));
            if (version.load() == v) { appliedVersion = v; break; }
        }
        for (auto& p : e.patterns) p.swingPct = static_cast<uint8_t>(*swing + 0.5f);
    }

private:
    void cache(const juce::String& idStr, std::atomic<float>*& dst) { dst = state.getRawParameterValue(idStr); jassert(dst != nullptr); }

    juce::AudioProcessorValueTreeState& state;
    std::atomic<float> *kitSel, *out, *seed, *sync, *play, *bpm, *lookahead, *pattern, *seqEnable, *swing, *nudge, *scatter, *velScatter;
    std::atomic<float>* padRaw[hic::kNumPads][id::PadParamCount];
    std::atomic<float> *bedType, *bedLevel, *bedDensity, *bedWarmth, *bedPop, *bedColor, *bedHiss, *bedGate, *bedGateAtt, *bedGateRel, *bedGateDuty;
    std::atomic<float> *duckDepth, *duckHold, *duckRel;
    std::atomic<float> *repOn, *repGrid, *repProb, *repLen, *repMinBars, *repMix;
    std::atomic<float> *frzHold, *frzGrain, *frzDensity, *frzSpray, *frzJitter, *frzMix;
    std::atomic<float> *revType, *revDecay, *revDamp, *revPre, *revMix;

    hic::KitParams kits[hic::KitCount];
    hic::Pattern editPatterns[hic::kNumPatterns];
    hic::Pattern shared[2][hic::kNumPatterns];
    std::atomic<int> activeSlot { 0 };
    std::atomic<int> version { 0 };
    int appliedVersion = -1;
    const hic::Engine* appliedEngine = nullptr;
};

} // namespace hicplug
