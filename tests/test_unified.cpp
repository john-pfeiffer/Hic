#include "Harness.h"
#include "Render.h"
#include "hic/Math.h"
#include "hic/Key.h"
#include "hic/Kit.h"
#include "hic/UnifiedVoice.h"
#include "hic/fx/BusShaper.h"
#include "hic/fx/Damp.h"
#include <vector>

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

static PadParams padWith(float tuneHz, float decayMs, float exciter, float body, float brk, float drift = 0.0f) {
    PadParams p;
    p.macro[MacroTune] = hzToTune(tuneHz); p.macro[MacroDecay] = msToDecay(decayMs);
    p.macro[MacroExciter] = exciter; p.macro[MacroBody] = body; p.macro[MacroBreak] = brk; p.macro[MacroDrift] = drift;
    return p;
}

static std::vector<float> renderVoice(UnifiedVoice& v, int frames, int block = 256) {
    std::vector<float> out((size_t)frames);
    for (int pos = 0; pos < frames; pos += block) v.render(out.data() + pos, std::min(block, frames - pos));
    return out;
}

TEST(fold_tri_is_identity_inside_and_bounded) {
    for (float x = -1.0f; x <= 1.0f; x += 0.01f) CHECK_NEAR(foldTri(x), x, 1e-6f);
    for (float x = -20.0f; x <= 20.0f; x += 0.013f) CHECK(foldTri(x) >= -1.0f && foldTri(x) <= 1.0f);
    CHECK_NEAR(foldTri(1.5f), 0.5f, 1e-6f);
    CHECK_NEAR(foldTri(3.0f), -1.0f, 1e-6f);
    CHECK_NEAR(foldTri(5.0f), 1.0f, 1e-6f);      // period 4
    CHECK_NEAR(smoothstep(0.5f, 0.0f, 1.0f), 0.5f, 1e-6f);
    CHECK(smoothstep(-1.0f, 0.0f, 1.0f) == 0.0f && smoothstep(2.0f, 0.0f, 1.0f) == 1.0f);
}

TEST(body_ratio_table_is_monotone_in_inharmonicity) {
    KeyParams key; UnifiedVoice v; v.prepare(kSr);
    static const float H[6] = { 1, 2, 3, 4, 5, 6 };
    float prev = -1.0f;
    for (float body = 0.5f; body <= 1.0001f; body += 0.05f) {
        v.trigger(padWith(200.0f, 200.0f, 0.3f, body, 0.0f), 0.8f, 60, 1u, key, 0.0f, 0.0f);
        float dist = 0.0f;
        for (int k = 0; k < 6; ++k) dist += std::fabs(std::log2(v.modeRatio(k)) - std::log2(H[k]));
        CHECK_MSG(dist >= prev - 1e-5f, "body %.2f distance %.3f < previous %.3f", double(body), double(dist), double(prev));
        prev = dist;
    }
    // Brightness (measured after the pitch bend has settled): membrane < wood < metal.
    auto bright = [&](float body) {
        v.trigger(padWith(200.0f, 300.0f, 0.3f, body, 0.0f), 0.8f, 60, 1u, key, 0.0f, 0.0f);
        auto s = renderVoice(v, 24000);
        const int from = int(0.08f * kSr);
        return bandRatioDb(s.data() + from, 24000 - from, kSr, 600.0f);   // three times f0
    };
    const float b0 = bright(0.0f), b5 = bright(0.5f), b1 = bright(1.0f);
    CHECK_MSG(b0 < b5 && b5 < b1, "brightness %.1f %.1f %.1f dB", double(b0), double(b5), double(b1));
}

TEST(body_crossfade_keeps_the_fundamental) {
    KeyParams key; UnifiedVoice v; v.prepare(kSr);
    for (float body : { 0.0f, 0.2f, 0.35f, 0.5f }) {   // the crossfade zone; beyond it the body is inharmonic by design
        v.trigger(padWith(200.0f, 400.0f, 0.3f, body, 0.0f), 0.8f, 60, 1u, key, 0.0f, 0.0f);
        auto s = renderVoice(v, 24000);
        const float f = estimateF0(s.data(), 24000, kSr, 40.0f, 60.0f);   // after the pitch drop settles
        CHECK_MSG(f > 180.0f && f < 220.0f, "body %.2f estimated f0 %.1f Hz", double(body), double(f));
    }
}

TEST(break_zero_is_transparent) {
    KeyParams key; UnifiedVoice a, b; a.prepare(kSr); b.prepare(kSr);
    b.setShaperForTest(false);
    PadParams p = padWith(300.0f, 200.0f, 0.5f, 0.6f, 0.0f);
    a.trigger(p, 0.8f, 60, 5u, key, 0.0f, 0.3f);
    b.trigger(p, 0.8f, 60, 5u, key, 0.0f, 0.3f);
    auto x = renderVoice(a, 12000), y = renderVoice(b, 12000);
    bool same = true; for (int i = 0; i < 12000; ++i) if (x[(size_t)i] != y[(size_t)i]) { same = false; break; }
    CHECK(same);
    // And Break changes the sound once it is up.
    a.trigger(padWith(300.0f, 200.0f, 0.5f, 0.6f, 0.6f), 0.8f, 60, 5u, key, 0.0f, 0.3f);
    auto z = renderVoice(a, 12000);
    bool diff = false; for (int i = 0; i < 12000; ++i) if (x[(size_t)i] != z[(size_t)i]) { diff = true; break; }
    CHECK(diff);
}

TEST(break_one_stays_bounded_and_dies) {
    KeyParams key;
    for (KitId id : { KitNeon, KitMicro, KitModular }) {
        KitParams kit; makeKit(id, kit);
        for (int pad = 0; pad < kNumPads; ++pad) {
            PadParams p = kit.pads[pad];
            p.macro[MacroBreak] = 1.0f; p.macro[MacroDecay] = msToDecay(1000.0f); p.macro[MacroDrift] = 0.0f;
            p.flags &= uint8_t(~PadFollowKey);
            UnifiedVoice v; v.prepare(kSr);
            v.trigger(p, 1.0f, 60, 77u, key, 0.0f, 1.0f);
            const int frames = int(kSr * 6.5f);
            auto s = renderVoice(v, frames, 512);
            CHECK_MSG(!hasNaN(s.data(), frames), "%s pad %d NaN at Break 1", kitName(id), pad);
            CHECK_MSG(peak(s.data(), frames) <= 1.3f, "%s pad %d peak %.2f at Break 1", kitName(id), pad, double(peak(s.data(), frames)));
            const float tail = rms(s.data() + int(kSr * 6.0f), int(kSr * 0.5f));
            CHECK_MSG(tail < 1e-4f, "%s pad %d tail rms %.2e after 6 s", kitName(id), pad, double(tail));
            CHECK_MSG(!v.isActive(), "%s pad %d still active after 6.5 s", kitName(id), pad);
        }
    }
}

TEST(feedback_is_deterministic_and_block_size_independent) {
    KeyParams key;
    PadParams p = padWith(120.0f, 800.0f, 0.9f, 0.35f, 1.0f);
    UnifiedVoice a, b, c; a.prepare(kSr); b.prepare(kSr); c.prepare(kSr);
    a.trigger(p, 0.9f, 60, 9u, key, 0.0f, 0.5f);
    b.trigger(p, 0.9f, 60, 9u, key, 0.0f, 0.5f);
    c.trigger(p, 0.9f, 60, 10u, key, 0.0f, 0.5f);
    const int frames = int(kSr * 3.0f);
    auto x = renderVoice(a, frames, 256), y = renderVoice(b, frames, 17), z = renderVoice(c, frames, 256);
    bool same = true, diff = false;
    for (int i = 0; i < frames; ++i) { if (x[(size_t)i] != y[(size_t)i]) same = false; if (x[(size_t)i] != z[(size_t)i]) diff = true; }
    CHECK(same);
    CHECK(diff);
}

TEST(key_quantize_table) {
    KeyParams cMajor{ 0, ScaleMajor }, cMinorPent{ 0, ScaleMinorPent }, aMinorPent{ 9, ScaleMinorPent }, chrom{ 0, ScaleChromatic };
    CHECK_NEAR(quantizeHz(440.0f, cMajor), 440.0f, 0.01f);
    CHECK_NEAR(quantizeHz(452.0f, cMinorPent), midiToHz(70.0f), 0.01f);    // A# above A
    CHECK_NEAR(quantizeHz(466.0f, aMinorPent), 440.0f, 0.01f);             // Bb is not in A minor pent, A is
    CHECK_NEAR(quantizeHz(450.0f, chrom), 440.0f, 0.01f);                  // chromatic snaps to the semitone
    CHECK_NEAR(quantizeHz(midiToHz(61.0f), cMajor), midiToHz(60.0f), 0.01f); // C# is a tie between C and D: down
    CHECK(inScale(ScaleDorian, 9) && !inScale(ScaleDorian, 8));
    CHECK(inScale(ScaleMinor, 8) && !inScale(ScaleMinor, 9));
}

TEST(keyed_pads_land_on_scale_degrees) {
    KeyParams key{ 0, ScaleMajor };
    PadParams p = padWith(300.0f, 400.0f, 0.3f, 0.6f, 0.0f);
    p.flags = PadFollowKey;
    UnifiedVoice v; v.prepare(kSr);
    v.trigger(p, 0.8f, 60, 1u, key, 0.0f, 0.0f);
    CHECK_NEAR(v.fundamentalHz(), midiToHz(62.0f), 0.01f);   // 300 Hz -> D4
    auto s = renderVoice(v, 24000);
    const float f = estimateF0(s.data(), 24000, kSr, 20.0f, 60.0f);
    CHECK_MSG(std::fabs(f - midiToHz(62.0f)) < midiToHz(62.0f) * 0.06f, "keyed pad f0 %.1f", double(f));
    // With drift, every hit still lands on a degree.
    p.macro[MacroDrift] = 0.3f;
    for (uint32_t seed = 1; seed < 40; ++seed) {
        v.trigger(p, 0.8f, 60, seed, key, 1.5f, 0.0f);
        const float midi = 69.0f + 12.0f * std::log2(v.fundamentalHz() / 440.0f);
        const int n = int(std::floor(midi + 0.5f));
        CHECK_MSG(std::fabs(midi - float(n)) < 0.01f && inScale(ScaleMajor, ((n % 12) + 12) % 12), "seed %u f0 %.2f Hz off scale", seed, double(v.fundamentalHz()));
    }
}

TEST(grit_follows_texture) {
    KeyParams key; UnifiedVoice a, b; a.prepare(kSr); b.prepare(kSr);
    PadParams p = padWith(500.0f, 100.0f, 0.2f, 0.7f, 0.0f);
    a.trigger(p, 0.8f, 60, 3u, key, 0.0f, 0.0f);
    b.trigger(p, 0.8f, 60, 3u, key, 0.0f, 0.6f);
    auto x = renderVoice(a, 12000), y = renderVoice(b, 12000);
    bool diff = false; for (int i = 0; i < 12000; ++i) if (x[(size_t)i] != y[(size_t)i]) { diff = true; break; }
    CHECK(diff);
    // In the engine, Texture at zero silences the static entirely.
    auto e = makeFullEngine(kSr);
    e->bus.texture = 0.0f; e->statik.levelDetail = 1.0f; e->repeat.enabled = false;
    auto s = mono(renderEvents(*e, {}, int(kSr), 64, 120.0));
    CHECK(peak(s.data(), int(kSr)) == 0.0f);
}

TEST(bus_stages_are_transparent_at_zero) {
    BusShaper sh; sh.set(0.0f);
    Damp dp; dp.prepare(kSr); dp.set(0.0f);
    Rng r(2); float l[512], rr[512], l0[512], r0[512];
    for (int i = 0; i < 512; ++i) { l[i] = l0[i] = r.bipolar(); rr[i] = r0[i] = r.bipolar(); }
    sh.process(l, rr, 512); dp.process(l, rr, 512);
    for (int i = 0; i < 512; ++i) { CHECK(l[i] == l0[i]); CHECK(rr[i] == r0[i]); }
    sh.set(0.6f); sh.process(l, rr, 512);
    bool diff = false; for (int i = 0; i < 512; ++i) if (l[i] != l0[i]) { diff = true; break; }
    CHECK(diff);
    CHECK(peak(l, 512) <= 1.0f);
    dp.set(0.8f);
    Rng n(4); std::vector<float> a((size_t)24000), b((size_t)24000);
    for (auto& v : a) v = n.bipolar(); b = a;
    dp.process(a.data(), b.data(), 24000);
    CHECK(bandRatioDb(a.data(), 24000, kSr, 4000.0f) < -12.0f);
}
