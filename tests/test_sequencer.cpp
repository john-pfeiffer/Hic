#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"
#include "hic/seq/Pattern.h"
#include "hic/seq/Sequencer.h"
#include <algorithm>

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

struct Abs { int64_t t; int pad; int vel; uint8_t flags; };

/// Runs the sequencer directly against a host clock and collects absolute event times.
static std::vector<Abs> run(const Pattern& p, const KitParams& kit, int block, double seconds, double bpm, uint32_t seed = 1,
                            bool stopAt = false, double stopSec = 0.0, double restartSec = 0.0) {
    Sequencer s; s.prepare(kSr);
    Clock clock; clock.prepare(kSr);
    std::vector<Abs> out;
    const int frames = int(seconds * double(kSr));
    NoteEvent ev[kMaxEvents];
    for (int pos = 0; pos < frames; pos += block) {
        const int n = std::min(block, frames - pos);
        TransportInfo t; t.valid = true; t.bpm = bpm;
        double sec = double(pos) / double(kSr);
        if (stopAt && sec >= stopSec && sec < restartSec) { t.playing = false; t.ppq = 0.0; }
        else if (stopAt && sec >= restartSec) { t.playing = true; t.ppq = bpm / 60.0 * (sec - restartSec); }
        else { t.playing = true; t.ppq = bpm / 60.0 * sec; }
        clock.update(t, n);
        const int c = s.process(p, kit, clock, n, seed, ev, kMaxEvents);
        for (int i = 0; i < c; ++i) out.push_back({ pos + ev[i].sampleTime, ev[i].pad, ev[i].vel, ev[i].flags });
    }
    std::sort(out.begin(), out.end(), [](const Abs& a, const Abs& b) { return a.t != b.t ? a.t < b.t : a.pad < b.pad; });
    return out;
}

static Pattern simplePattern() {
    Pattern p; clearPattern(p);
    for (int i = 0; i < 16; ++i) { p.tracks[PadClosedHat].steps[i].on = 1; p.tracks[PadClosedHat].steps[i].vel = 100; }
    p.tracks[PadKick].steps[0].on = 1; p.tracks[PadKick].steps[8].on = 1;
    return p;
}

TEST(event_times_do_not_depend_on_block_size) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p; makeDemoPattern(p);
    for (auto& t : p.tracks) for (auto& s : t.steps) s.prob = 100;   // deterministic content
    auto ref = run(p, kit, 4096, 6.0, 92.0);
    CHECK(ref.size() > 40);
    for (int block : { 17, 64, 1000 }) {
        auto got = run(p, kit, block, 6.0, 92.0);
        CHECK_MSG(got.size() == ref.size(), "block %d: %zu events vs %zu", block, got.size(), ref.size());
        for (size_t i = 0; i < std::min(got.size(), ref.size()); ++i) {
            CHECK_MSG(got[i].t == ref[i].t && got[i].pad == ref[i].pad, "block %d event %zu at %lld vs %lld", block, i, (long long)got[i].t, (long long)ref[i].t);
            if (got[i].t != ref[i].t) break;
        }
    }
}

TEST(steps_land_on_the_grid_and_nudge_moves_them) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p = simplePattern();
    p.tracks[PadClosedHat].steps[2].nudgeMs = 20;
    p.tracks[PadClosedHat].steps[3].nudgeMs = -20;
    auto ev = run(p, kit, 64, 1.0, 120.0);
    const double step = 60.0 / 120.0 / 4.0 * double(kSr);   // 6000 samples
    std::vector<Abs> hats; for (auto& e : ev) if (e.pad == PadClosedHat) hats.push_back(e);
    CHECK_MSG(hats.size() == 8, "%zu hats in 1 s at 120 BPM", hats.size());
    CHECK_NEAR(hats[0].t, 0, 1);
    CHECK_NEAR(hats[1].t, step, 1);
    CHECK_NEAR(hats[2].t, 2 * step + 0.020 * double(kSr), 1);
    CHECK_NEAR(hats[3].t, 3 * step - 0.020 * double(kSr), 1);
    CHECK_NEAR(hats[4].t, 4 * step, 1);
}

TEST(swing_delays_odd_steps) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p = simplePattern(); p.swingPct = 66;
    auto ev = run(p, kit, 64, 1.0, 120.0);
    std::vector<Abs> hats; for (auto& e : ev) if (e.pad == PadClosedHat) hats.push_back(e);
    const double step = 6000.0;
    CHECK_NEAR(hats[0].t, 0, 1);
    CHECK_NEAR(hats[1].t, step + (0.66 * 2.0 - 1.0) * step, 2);
    CHECK_NEAR(hats[2].t, 2 * step, 1);
}

TEST(probability_is_deterministic_per_seed) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p = simplePattern();
    for (auto& s : p.tracks[PadClosedHat].steps) s.prob = 50;
    auto a = run(p, kit, 256, 16.0, 120.0, 7);
    auto b = run(p, kit, 256, 16.0, 120.0, 7);
    auto c = run(p, kit, 256, 16.0, 120.0, 8);
    CHECK(a.size() == b.size());
    bool same = a.size() == b.size();
    for (size_t i = 0; same && i < a.size(); ++i) same = a[i].t == b[i].t;
    CHECK(same);
    CHECK(a.size() != c.size() || a.front().t != c.front().t || a.back().t != c.back().t);
    int hats = 0; for (auto& e : a) hats += e.pad == PadClosedHat;
    CHECK_MSG(hats > 128 * 0.3 && hats < 128 * 0.7, "%d of 128 hats fired at 50 %%", hats);
    for (auto& s : p.tracks[PadClosedHat].steps) s.prob = 0;
    auto z = run(p, kit, 256, 4.0, 120.0);
    int zh = 0; for (auto& e : z) zh += e.pad == PadClosedHat;
    CHECK(zh == 0);
}

TEST(ratchet_subdivides_the_step) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p; clearPattern(p);
    p.tracks[PadRide].steps[4].on = 1; p.tracks[PadRide].steps[4].ratchet = 3; p.tracks[PadRide].steps[4].vel = 100;
    auto ev = run(p, kit, 64, 1.5, 120.0);
    CHECK_MSG(ev.size() == 3, "%zu events for a ratchet of 3", ev.size());
    if (ev.size() == 3) {
        CHECK_NEAR(ev[0].t, 4 * 6000, 1);
        CHECK_NEAR(ev[1].t, 4 * 6000 + 2000, 1);
        CHECK_NEAR(ev[2].t, 4 * 6000 + 4000, 1);
        CHECK(ev[0].vel > ev[1].vel && ev[1].vel > ev[2].vel);
    }
}

TEST(reverse_steps_fire_early_with_the_flag) {
    KitParams kit; makeDefaultKit(kit);
    kit.pads[PadOpenHat].reverseMs = 120.0f;
    Pattern p; clearPattern(p);
    p.tracks[PadOpenHat].steps[8].on = 1; p.tracks[PadOpenHat].steps[8].flags = StepReverse;
    auto ev = run(p, kit, 64, 2.0, 120.0);
    CHECK(ev.size() == 1);
    if (!ev.empty()) {
        CHECK_NEAR(ev[0].t, 8 * 6000 - 0.120 * double(kSr), 1);
        CHECK(ev[0].flags & EvReverse);
    }
}

TEST(stop_and_restart_never_double_fires) {
    KitParams kit; makeDefaultKit(kit);
    Pattern p = simplePattern();
    // Play 1 s, stop for 0.5 s, restart from zero for 1 s.
    auto ev = run(p, kit, 64, 2.5, 120.0, 1, true, 1.0, 1.5);
    std::vector<int64_t> hats; for (auto& e : ev) if (e.pad == PadClosedHat) hats.push_back(e.t);
    CHECK_MSG(hats.size() == 16, "%zu hats over play/stop/play", hats.size());
    for (size_t i = 1; i < hats.size(); ++i) CHECK(hats[i] > hats[i - 1]);
    // Restart lands on step 0 exactly at 1.5 s.
    bool restart = false; for (auto t : hats) if (std::llabs(t - int64_t(1.5 * double(kSr))) <= 1) restart = true;
    CHECK(restart);

    // A loop jump (ppq going backwards mid-block) behaves the same way.
    Sequencer s; s.prepare(kSr); Clock clock; clock.prepare(kSr);
    NoteEvent buf[kMaxEvents]; int total = 0;
    for (int b = 0; b < 200; ++b) {
        TransportInfo t; t.valid = true; t.playing = true; t.bpm = 120.0;
        t.ppq = 2.0 * (double(b * 64) / double(kSr));
        if (b >= 100) t.ppq -= 2.0 * (double(100 * 64) / double(kSr));   // jump back to zero at block 100
        clock.update(t, 64);
        total += s.process(p, kit, clock, 64, 1, buf, kMaxEvents);
    }
    // 100 blocks of 64 = 0.133 s each half -> hats at 0 and 0.125 s plus a kick at 0, twice over.
    CHECK_MSG(total == 6, "%d events over a loop jump", total);
}

TEST(sequencer_and_midi_share_the_engine) {
    auto e = makeEngine(kSr);
    clearPattern(e->patterns[0]);
    e->patterns[0].tracks[PadSnare].steps[0].on = 1;
    e->global.seqEnabled = true;
    renderEvents(*e, { midiHit(0, 36, 0.9f) }, 64, 64, 120.0);
    CHECK(e->activeVoices(PadKick) == 1);
    CHECK(e->activeVoices(PadSnare) == 1);
}

TEST(internal_clock_runs_the_pattern_without_a_host) {
    auto e = makeEngine(kSr);
    e->patterns[0] = simplePattern();
    for (auto& s : e->patterns[0].tracks[PadClosedHat].steps) s.on = 0;
    e->patterns[0].tracks[PadClosedHat].steps[0].on = 1;   // one click per bar
    e->kit.pads[PadKick].level = 0.0f;
    e->global.internalPlay = true; e->global.internalBpm = 120.0;
    const int frames = int(kSr * 4.0f);
    auto s = mono(renderEvents(*e, {}, frames, 64));   // no host transport
    int onsets = 0; int quiet = 1 << 20;
    for (int i = 0; i < frames; ++i) { if (std::fabs(s[size_t(i)]) > 1e-3f) { if (quiet > 2000) ++onsets; quiet = 0; } else ++quiet; }
    CHECK_MSG(onsets == 2, "%d bars in 4 s at 120 BPM (expected 2)", onsets);
    CHECK(e->currentStep(PadClosedHat) >= 0);
    e->global.internalPlay = false;
    renderEvents(*e, {}, 64, 64);
    CHECK(e->currentStep(PadClosedHat) == -1);
}

TEST(static_follows_step_flags) {
    auto e = makeFullEngine(kSr);
    clearPattern(e->patterns[0]);
    Track& t = e->patterns[0].tracks[PadClosedHat];
    for (int i = 0; i < 16; ++i) { t.steps[i].on = 1; t.steps[i].vel = 1; t.steps[i].flags = (i < 8) ? StepBedGate : 0; }
    e->kit.pads[PadClosedHat].level = 0.0f;
    e->bus = BusParams{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    e->statik.levelDetail = 1.0f; e->statik.colour = 1.0f; e->statik.clock = StaticClock::Steps; e->statik.attackMs = 1.0f; e->statik.releaseMs = 5.0f;
    e->repeat.enabled = false;
    const int frames = int(kSr * 2.0f);
    auto s = mono(renderEvents(*e, {}, frames, 64, 120.0));
    const int bar = int(2.0 * double(kSr));
    const float open = rms(s.data() + 2000, bar / 2 - 4000);
    const float closed = rms(s.data() + bar / 2 + 2000, bar / 2 - 4000);
    CHECK_MSG(open > closed * 5.0f, "gate open %.4f closed %.4f", double(open), double(closed));
}
