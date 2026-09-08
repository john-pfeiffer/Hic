#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"
#include "hic/fx/Ducker.h"
#include "hic/fx/SpringReverb.h"
#include "hic/fx/BeatRepeat.h"
#include "hic/fx/GranularFreeze.h"
#include "hic/StaticTexture.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

TEST(feel_is_deterministic_per_seed) {
    auto make = [](uint32_t seed) {
        auto e = makeEngine(kSr);
        e->bus.feel = 1.0f; e->feel.scatterMs = 6.0f; e->feel.velScatter = 0.3f; e->feel.lookaheadMs = 20.0f; e->feel.seed = seed;
        e->prepare(kSr);
        return e;
    };
    std::vector<NoteEvent> ev;
    for (int i = 0; i < 16; ++i) ev.push_back(midiHit(i * 6000, i % 2 ? 42 : 36, 0.8f));
    auto a = mono(renderEvents(*make(5), ev, 96000, 64));
    auto b = mono(renderEvents(*make(5), ev, 96000, 64));
    auto c = mono(renderEvents(*make(6), ev, 96000, 64));
    bool same = true, diff = false;
    for (size_t i = 0; i < a.size(); ++i) { if (a[i] != b[i]) same = false; if (a[i] != c[i]) diff = true; }
    CHECK(same);
    CHECK(diff);
}

TEST(scatter_moves_hits_within_lookahead) {
    auto e = makeEngine(kSr);
    e->bus.feel = 1.0f; e->feel.scatterMs = 8.0f; e->feel.lookaheadMs = 20.0f; e->prepare(kSr);
    e->kit.pads[PadClosedHat].scatterMul = 1.0f;
    int minFirst = 1 << 30, maxFirst = -1;
    for (int i = 0; i < 12; ++i) {
        e->feel.seed = uint32_t(i + 1);
        auto s = mono(renderEvents(*e, { hit(0, PadClosedHat, 0.9f) }, 4800, 64));
        const int f = firstIndexAbove(s.data(), 4800, 1e-4f);
        minFirst = std::min(minFirst, f); maxFirst = std::max(maxFirst, f);
    }
    const int la = int(0.020f * kSr);
    CHECK_MSG(minFirst < la && maxFirst > la, "hits spread %d..%d around lookahead %d", minFirst, maxFirst, la);
    CHECK(minFirst >= la - int(0.008f * kSr) - 2);
}

TEST(ducker_dips_to_depth_and_recovers) {
    Ducker d; d.prepare(kSr);
    DuckParams p; p.depthDb = -12.0f; p.attackMs = 2.0f; p.holdMs = 0.0f; p.releaseMs = 100.0f;
    d.set(p);
    float g = d.tick();
    CHECK_NEAR(g, 1.0f, 1e-6f);
    d.trigger();
    float mn = 1.0f;
    for (int i = 0; i < 480; ++i) mn = std::min(mn, d.tick());
    CHECK_NEAR(gainToDb(mn), -12.0f, 0.5f);
    for (int i = 0; i < int(0.100f * kSr); ++i) g = d.tick();
    CHECK(g > 0.99f);
}

TEST(reverb_decays_on_time_and_goes_silent) {
    for (ReverbType type : { ReverbType::Spring, ReverbType::Room }) {
        SpringReverb r; r.prepare(kSr);
        ReverbParams p; p.type = type; p.decaySec = 0.6f; p.dampHz = 6000.0f;
        r.set(p);
        const int n = int(kSr * 3.0f);
        std::vector<float> send((size_t)n, 0.0f), l((size_t)n, 0.0f), rr((size_t)n, 0.0f);
        send[0] = 1.0f;
        r.process(send.data(), l.data(), rr.data(), n);
        CHECK(!hasNaN(l.data(), n));
        CHECK(peak(l.data(), n) < 1.0f);
        // The damping shortens the highs on purpose; the decay setting describes the
        // low band. Measure the tail from 50 ms on through an 800 Hz lowpass.
        TptSvf lp1, lp2; lp1.set(800.0f, 0.707f, kSr); lp2.set(800.0f, 0.707f, kSr);
        std::vector<float> low((size_t)n);
        for (int i = 0; i < n; ++i) low[(size_t)i] = lp2.lp(lp1.lp(l[(size_t)i]));
        const int from = int(0.05f * kSr);
        const float dec = decayMs(low.data() + from, n - from, kSr, -60.0f);
        CHECK_MSG(dec > 0.6f * 1000.0f * 0.7f && dec < 0.6f * 1000.0f * 1.3f, "reverb tail %.0f ms for 600 ms setting", double(dec));
        CHECK(lastIndexAbove(l.data(), n, 1e-5f) < int(kSr * 2.5f));
    }
}

TEST(beat_repeat_is_rare_and_spaced) {
    BeatRepeat br; br.prepare(kSr);
    Clock clock; clock.prepare(kSr);
    RepeatParams p; p.grid = 32; p.probability = 0.25f; p.lengthBeats = 0.25f; p.minBarsBetween = 1;
    TransportInfo t; t.valid = true; t.playing = true; t.bpm = 120.0;
    Rng noise(3);
    const int block = 256;
    const double totalBeats = 256 * 4;
    const int frames = int(totalBeats * 60.0 / 120.0 * double(kSr));
    int64_t prevBar = -100; bool spacingOk = true; int64_t prevCount = 0;
    for (int pos = 0; pos < frames; pos += block) {
        t.ppq = 120.0 / 60.0 * double(pos) / double(kSr);
        clock.update(t, block);
        float l[block], r[block];
        for (int i = 0; i < block; ++i) { l[i] = noise.bipolar(); r[i] = l[i]; }
        br.process(l, r, block, p, clock, 42u);
        if (br.repeatCount() != prevCount) {
            prevCount = br.repeatCount();
            const int64_t bar = br.lastRepeatBar();
            if (bar - prevBar < p.minBarsBetween) spacingOk = false;
            prevBar = bar;
        }
    }
    CHECK_MSG(br.repeatCount() >= 40 && br.repeatCount() <= 95, "repeat fired %d times in 256 bars (expect ~64)", br.repeatCount());
    CHECK(spacingOk);

    // Deterministic per seed.
    BeatRepeat br2; br2.prepare(kSr);
    for (int pos = 0; pos < frames; pos += block) {
        t.ppq = 120.0 / 60.0 * double(pos) / double(kSr);
        clock.update(t, block);
        float l[block], r[block];
        for (int i = 0; i < block; ++i) { l[i] = 0.0f; r[i] = 0.0f; }
        br2.process(l, r, block, p, clock, 42u);
    }
    CHECK(br2.repeatCount() == br.repeatCount());
}

TEST(beat_repeat_loops_the_slice) {
    BeatRepeat br; br.prepare(kSr);
    Clock clock; clock.prepare(kSr);
    RepeatParams p; p.grid = 32; p.probability = 0.0f; p.lengthBeats = 1.0f; p.mix = 1.0f;
    TransportInfo t; t.valid = true; t.playing = true; t.bpm = 120.0;
    const int sliceLen = int(4.0 / 32 * 60.0 / 120.0 * double(kSr));   // 3000 samples
    const int frames = 48000;
    std::vector<float> l((size_t)frames), r((size_t)frames);
    Rng noise(9);
    for (int i = 0; i < frames; ++i) l[size_t(i)] = r[size_t(i)] = noise.bipolar();
    for (int pos = 0; pos < frames; pos += 256) {
        t.ppq = 120.0 / 60.0 * double(pos) / double(kSr);
        clock.update(t, 256);
        if (pos == 256 * 20) br.force();
        br.process(l.data() + pos, r.data() + pos, 256, p, clock, 1u);
    }
    CHECK(br.repeatCount() == 1);
    // Mid-repeat, the output must repeat with the slice period.
    const int start = 256 * 20 + sliceLen + 200;
    float diff = 0.0f;
    for (int i = start; i < start + 1000; ++i) diff = std::max(diff, std::fabs(l[size_t(i)] - l[size_t(i - sliceLen)]));
    CHECK_MSG(diff < 1e-5f, "slice not periodic, diff %g", double(diff));
}

TEST(freeze_is_silent_when_off_and_sustains_when_held) {
    GranularFreeze f; f.prepare(kSr);
    FreezeParams p; p.hold = false; p.mix = 1.0f;
    const int n = 4800;
    std::vector<float> tap((size_t)n), l((size_t)n, 0.0f), r((size_t)n, 0.0f);
    Rng noise(4);
    for (auto& x : tap) x = noise.bipolar() * 0.5f;
    f.process(tap.data(), l.data(), r.data(), n, p);
    CHECK(peak(l.data(), n) == 0.0f);
    // Hold: feed silence afterwards and expect sound for as long as it is held.
    p.hold = true;
    std::vector<float> silence((size_t)n, 0.0f);
    float pk = 0.0f;
    for (int b = 0; b < 10; ++b) {
        std::fill(l.begin(), l.end(), 0.0f); std::fill(r.begin(), r.end(), 0.0f);
        f.process(silence.data(), l.data(), r.data(), n, p);
        if (b == 9) pk = peak(l.data(), n) + peak(r.data(), n);
    }
    CHECK_MSG(pk > 0.01f, "freeze silent after 1 s hold, peak %g", double(pk));
    CHECK(!hasNaN(l.data(), n));
    p.hold = false;
    for (int b = 0; b < 3; ++b) { std::fill(l.begin(), l.end(), 0.0f); f.process(silence.data(), l.data(), r.data(), n, p); }
    CHECK(peak(l.data(), n) == 0.0f);
}

TEST(static_pulses_land_on_the_clock) {
    auto e = makeFullEngine(kSr);
    e->bus = BusParams{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    e->statik.levelDetail = 1.0f; e->statik.clock = StaticClock::Sixteenths; e->statik.pulseMs = 20.0f;
    e->statik.attackMs = 1.0f; e->statik.releaseMs = 5.0f; e->statik.colour = 0.5f;
    e->repeat.enabled = false;
    const int frames = int(kSr * 4.0f);
    Stereo st = renderEvents(*e, {}, frames, 64, 120.0);
    auto s = mono(st);
    const int step = int(0.125 * double(kSr));   // a sixteenth at 120 BPM
    for (int k = 1; k < 15; ++k) {
        const int onset = firstIndexAbove(s.data() + k * step - 200, 400, 2e-3f) + k * step - 200;
        CHECK_MSG(std::abs(onset - k * step) <= int(0.001f * kSr), "pulse %d onset %d vs %d", k, onset, k * step);
        const float on = rms(s.data() + k * step + 100, int(0.015f * kSr));
        const float off = rms(s.data() + k * step + int(0.06f * kSr), int(0.05f * kSr));
        CHECK_MSG(on > off * 10.0f, "pulse %d on %.4f off %.4f", k, double(on), double(off));
    }
    bool stereoDiffers = false;
    for (int i = 0; i < frames; ++i) if (st.l[size_t(i)] != st.r[size_t(i)]) { stereoDiffers = true; break; }
    CHECK(stereoDiffers);
    // Bar 2 equals bar 1: pulses are seeded from their position in the bar.
    const int bar = 16 * step;
    float maxDiff = 0.0f;
    for (int i = 0; i < bar; ++i) maxDiff = std::max(maxDiff, std::fabs(s[size_t(i)] - s[size_t(i + bar)]));
    CHECK_MSG(maxDiff < 1e-5f, "bars differ by %g", double(maxDiff));
    CHECK(!hasNaN(s.data(), frames));
}

TEST(static_ducks_to_the_kick) {
    auto d = makeFullEngine(kSr);
    d->bus = BusParams{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    d->statik.levelDetail = 1.0f; d->statik.clock = StaticClock::Quarters; d->statik.pulseMs = 2000.0f; d->statik.colour = 1.0f;
    d->duck.depthDb = -20.0f; d->duck.holdMs = 60.0f; d->duck.releaseMs = 200.0f;
    d->repeat.enabled = false;
    d->kit.pads[PadKick].level = 0.0f;   // silent kick, only the duck remains
    const int frames = int(kSr * 2.0f);
    auto k = mono(renderEvents(*d, { hit(int(0.5f * kSr), PadKick, 1.0f) }, frames, 64, 120.0));
    const float before = rms(k.data() + int(0.3f * kSr), int(0.1f * kSr));
    const float during = rms(k.data() + int(0.505f * kSr), int(0.05f * kSr));
    const float after = rms(k.data() + int(1.5f * kSr), int(0.1f * kSr));
    CHECK_MSG(during < before * 0.2f, "duck before %.4f during %.4f", double(before), double(during));
    CHECK(after > before * 0.8f);
}

TEST(control_notes_hold_freeze_and_force_repeat) {
    auto e = makeFullEngine(kSr);
    e->bus.texture = 0.0f; e->bus.space = 0.0f;
    e->repeat.probability = 0.0f; e->repeat.lengthBeats = 0.5f;
    e->freeze.mix = 1.0f;
    NoteEvent holdOn = midiHit(int(0.3f * kSr), kNoteFreezeHold, 1.0f);
    NoteEvent holdOff = midiHit(int(1.2f * kSr), kNoteFreezeHold, 1.0f); holdOff.flags = EvNoteOff;
    NoteEvent force = midiHit(int(1.5f * kSr), kNoteForceRepeat, 1.0f);
    const int frames = int(kSr * 2.0f);
    auto s = mono(renderEvents(*e, { hit(int(0.1f * kSr), PadClosedHat, 0.9f), hit(int(0.2f * kSr), PadClosedHat, 0.9f), holdOn, holdOff, force }, frames, 64, 120.0));
    CHECK(e->activeVoices() == 0);
    // Sound while held (0.5..1.1 s) even though no pad plays; quiet after release.
    CHECK_MSG(rms(s.data() + int(0.5f * kSr), int(0.6f * kSr)) > 1e-4f, "freeze produced nothing");
    CHECK(rms(s.data() + int(1.3f * kSr), int(0.15f * kSr)) < 1e-5f);
    CHECK(e->beatRepeat().repeatCount() == 1);
}
