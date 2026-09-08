#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"
#include "hic/UnifiedVoice.h"
#include <utility>

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

static int auditionNote(int pad) { return pad == PadGlock ? 72 : (pad == PadThumb ? 45 : 36); }

static void checkKitClean(KitId id) {
    auto e = makeEngine(kSr);
    makeKit(id, e->kit);
    const int frames = int(kSr * 3.0f);
    for (int pad = 0; pad < kNumPads; ++pad) {
        const PadParams& p = e->kit.pads[pad];
        const float T = decayToMs(p.macro[MacroDecay]);
        const float fb = p.macro[MacroBreak] > 0.5f ? 1.5f * T : 0.0f;   // feedback keeps the tail alive
        const float lo = pad == PadGlock ? 600.0f : 0.5f * T;
        const float hi = pad == PadGlock ? 5000.0f : 2.5f * T + 60.0f + fb;
        for (float vel : { 0.3f, 0.7f, 1.0f }) {
            Stereo s = renderEvents(*e, { hit(0, pad, vel, auditionNote(pad)) }, frames);
            auto m = mono(s);
            const float pk = peak(m.data(), frames);
            const float dec = decayMs(m.data(), frames, kSr);
            CHECK_MSG(!hasNaN(s.l.data(), frames) && !hasNaN(s.r.data(), frames), "%s pad %d NaN", kitName(id), pad);
            CHECK_MSG(pk > 0.01f, "%s pad %d silent at vel %.1f", kitName(id), pad, double(vel));
            CHECK_MSG(pk <= dbToGain(-0.5f), "%s pad %d (%s) vel %.1f peak %.2f dBFS", kitName(id), pad, defaultPadName(pad), double(vel), double(gainToDb(pk)));
            CHECK_MSG(dec >= lo && dec <= hi, "%s pad %d (%s) vel %.1f decay %.1f ms not in [%g, %g]",
                      kitName(id), pad, defaultPadName(pad), double(vel), double(dec), double(lo), double(hi));
            CHECK_MSG(e->activeVoices() == 0 || pad == PadGlock, "%s pad %d still active after 3 s", kitName(id), pad);
        }
    }
}

TEST(neon_kit_is_clean_and_in_its_decay_windows) { checkKitClean(KitNeon); }
TEST(micro_kit_is_clean) { checkKitClean(KitMicro); }
TEST(modular_kit_is_clean) { checkKitClean(KitModular); }

TEST(velocity_changes_level) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr);
    for (int pad = 0; pad < kNumPads; ++pad) {
        auto soft = mono(renderEvents(*e, { hit(0, pad, 0.3f, auditionNote(pad)) }, frames));
        auto loud = mono(renderEvents(*e, { hit(0, pad, 1.0f, auditionNote(pad)) }, frames));
        CHECK_MSG(peak(loud.data(), frames) > peak(soft.data(), frames) * 1.3f, "pad %d velocity has no effect", pad);
    }
}

TEST(kick_has_no_top_end_and_hats_have_no_bottom) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr);
    auto kick = mono(renderEvents(*e, { hit(0, PadKick, 0.9f) }, frames));
    CHECK_MSG(bandRatioDb(kick.data(), frames, kSr, 4000.0f) < -24.0f, "kick top end %.1f dB", double(bandRatioDb(kick.data(), frames, kSr, 4000.0f)));
    auto thumb = mono(renderEvents(*e, { hit(0, PadThumb, 0.9f, 45) }, frames));
    CHECK(bandRatioDb(thumb.data(), frames, kSr, 4000.0f) < -24.0f);
    for (auto pr : { std::pair<int, float>{ PadOpenHat, 12.0f }, std::pair<int, float>{ PadPedalHat, 12.0f }, std::pair<int, float>{ PadClosedHat, 6.0f } }) {
        auto h = mono(renderEvents(*e, { hit(0, pr.first, 0.9f) }, frames));
        const float r = bandRatioDb(h.data(), frames, kSr, 2000.0f);
        CHECK_MSG(r > pr.second, "pad %d bottom end ratio %.1f dB", pr.first, double(r));
    }
}

TEST(glock_follows_the_note) {
    auto e = makeEngine(kSr);
    e->kit.pads[PadGlock].flags &= uint8_t(~PadFollowKey);
    const int frames = int(kSr * 0.5f);
    auto lo = mono(renderEvents(*e, { hit(0, PadGlock, 0.9f, 60) }, frames));
    auto hi = mono(renderEvents(*e, { hit(0, PadGlock, 0.9f, 72) }, frames));
    const float f1 = estimateF0(lo.data(), frames, kSr), f2 = estimateF0(hi.data(), frames, kSr);
    CHECK_MSG(f1 > 0.0f && f2 > f1 * 1.8f && f2 < f1 * 2.2f, "f0 lo %.0f hi %.0f", double(f1), double(f2));
    CHECK_NEAR(f1, 1046.0f * 0.5f, 1046.0f * 0.5f * 0.08f);   // base note 72 at 1046 Hz, so note 60 is an octave down
}

TEST(reverse_swells_into_the_end) {
    auto e = makeEngine(kSr);
    e->kit.pads[PadOpenHat].flags |= PadReverse;
    e->kit.pads[PadOpenHat].reverseMs = 150.0f;
    const int frames = int(kSr * 0.5f);
    auto s = mono(renderEvents(*e, { hit(0, PadOpenHat, 0.9f) }, frames));
    const int len = int(0.150f * kSr);
    const float early = rms(s.data(), len / 3);
    const float late = rms(s.data() + 2 * len / 3, len / 3);
    CHECK_MSG(late > early * 2.0f, "reverse early %.4f late %.4f", double(early), double(late));
    CHECK(lastIndexAbove(s.data(), frames, 1e-4f) < len + 64);
}

TEST(drift_varies_hits_deterministically) {
    KeyParams key;
    PadParams p; p.macro[MacroDrift] = 0.5f;
    UnifiedVoice a, b, c; a.prepare(kSr); b.prepare(kSr); c.prepare(kSr);
    a.trigger(p, 0.9f, 60, 11u, key, 1.0f, 0.0f);
    b.trigger(p, 0.9f, 60, 12u, key, 1.0f, 0.0f);
    c.trigger(p, 0.9f, 60, 11u, key, 1.0f, 0.0f);
    bool differs = false, same = true;
    for (int i = 0; i < kNumMacros; ++i) {
        if (a.effectiveMacros()[i] != b.effectiveMacros()[i]) differs = true;
        if (a.effectiveMacros()[i] != c.effectiveMacros()[i]) same = false;
    }
    CHECK(differs);
    CHECK(same);
    // Bus drift at zero: the macros do not move whatever the seed.
    a.trigger(p, 0.9f, 60, 11u, key, 0.0f, 0.0f);
    b.trigger(p, 0.9f, 60, 12u, key, 0.0f, 0.0f);
    for (int i = 0; i < kNumMacros; ++i) CHECK(a.effectiveMacros()[i] == b.effectiveMacros()[i]);
    // Through the engine: same seed, same audio; second hit differs (new seed).
    auto e = makeEngine(kSr); e->bus.drift = 1.0f; e->kit.pads[PadWoodblock].macro[MacroDrift] = 0.5f;
    const int frames = int(kSr * 0.5f);
    auto x = mono(renderEvents(*e, { hit(0, PadWoodblock, 0.9f) }, frames));
    auto y = mono(renderEvents(*e, { hit(0, PadWoodblock, 0.9f) }, frames));
    bool d2 = false; for (int i = 0; i < frames; ++i) if (x[size_t(i)] != y[size_t(i)]) { d2 = true; break; }
    CHECK(d2);
}
