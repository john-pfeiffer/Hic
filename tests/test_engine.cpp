#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

TEST(default_kit_maps_every_note) {
    KitParams kit; makeDefaultKit(kit);
    for (int n = 0; n < 128; ++n) CHECK(kit.noteToPad[n] < kNumPads);
    CHECK(kit.noteToPad[36] == PadKick);
    CHECK(kit.noteToPad[38] == PadSnare);
    CHECK(kit.noteToPad[42] == PadClosedHat);
    CHECK(kit.noteToPad[46] == PadOpenHat);
    CHECK(kit.noteToPad[37] == PadSideStick);
    CHECK(kit.noteToPad[0] == PadKick);        // nearest fallback
    CHECK(kit.noteToPad[100] == PadGlock);
}

TEST(closed_hat_chokes_open_hat) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr * 0.3f);
    std::vector<NoteEvent> ev = { hit(0, PadOpenHat, 0.9f), hit(int(0.05f * kSr), PadClosedHat, 0.9f) };
    TransportInfo t;
    NoteEvent in[2];
    std::vector<float> l(size_t(frames), 0.0f), r(size_t(frames), 0.0f);
    int openActiveAfter = -1;
    for (int pos = 0; pos < frames; pos += 64) {
        int nIn = 0;
        for (auto& x : ev) if (x.sampleTime >= pos && x.sampleTime < pos + 64) { in[nIn] = x; in[nIn].sampleTime -= pos; ++nIn; }
        e->process(in, nIn, t, l.data() + pos, r.data() + pos, 64);
        if (pos >= int(0.05f * kSr) + int(0.005f * kSr) && openActiveAfter < 0) openActiveAfter = e->activeVoices(PadOpenHat);
    }
    CHECK_MSG(openActiveAfter == 0, "open hat still active %d after choke", openActiveAfter);
}

TEST(voice_pool_never_allocates_or_overflows) {
    auto e = makeEngine(kSr);
    std::vector<NoteEvent> ev;
    for (int i = 0; i < 40; ++i) ev.push_back(hit(i, PadGlock, 0.8f, 60 + (i % 12)));
    for (int i = 0; i < 40; ++i) ev.push_back(hit(i, i % kNumPads, 0.8f, 40 + i));
    TransportInfo t;
    NoteEvent in[kMaxEvents];
    float l[256], r[256];
    int nIn = 0;
    for (auto& x : ev) if (nIn < kMaxEvents) { in[nIn++] = x; }
    {
        NO_ALLOC_ZONE();
        e->process(in, nIn, t, l, r, 256);
        for (int b = 0; b < 100; ++b) e->process(nullptr, 0, t, l, r, 256);
    }
    CHECK(e->activeVoices() <= kNumVoices);
    CHECK(!hasNaN(l, 256));
}

TEST(polyphony_limit_is_respected) {
    auto e = makeEngine(kSr);
    e->kit.pads[PadGlock].maxPoly = 3;
    std::vector<NoteEvent> ev;
    for (int i = 0; i < 6; ++i) ev.push_back(hit(i * 10, PadGlock, 0.8f, 60 + i));
    renderEvents(*e, ev, 512, 256);
    CHECK_MSG(e->activeVoices(PadGlock) <= 3, "glock voices %d", e->activeVoices(PadGlock));
    CHECK(e->activeVoices(PadKick) == 0);
}

TEST(midi_notes_route_through_the_kit_map) {
    auto e = makeEngine(kSr);
    renderEvents(*e, { midiHit(0, 36, 0.9f), midiHit(0, 42, 0.9f) }, 64, 64);
    CHECK(e->activeVoices(PadKick) == 1);
    CHECK(e->activeVoices(PadClosedHat) == 1);
}

TEST(decay_is_independent_of_sample_rate) {
    float ref = 0.0f;
    for (float sr : { 44100.0f, 48000.0f, 96000.0f }) {
        auto e = makeEngine(sr);
        const int frames = int(sr * 1.5f);
        auto k = mono(renderEvents(*e, { hit(0, PadKick, 0.9f) }, frames));
        const float d = decayMs(k.data(), frames, sr, -40.0f);
        if (ref == 0.0f) ref = d;
        CHECK_MSG(std::fabs(d - ref) <= ref * 0.05f + 1.0f, "kick decay %.1f ms at %.0f Hz vs %.1f", double(d), double(sr), double(ref));
    }
}

TEST(lookahead_delays_hits_and_lets_them_move_early) {
    auto e = makeEngine(kSr);
    e->feel.lookaheadMs = 20.0f;
    e->feel.nudgeMs = 0.0f;
    e->prepare(kSr);
    const int frames = int(kSr * 0.2f);
    auto s = mono(renderEvents(*e, { hit(0, PadClosedHat, 0.9f) }, frames, 64));
    const int first = firstIndexAbove(s.data(), frames, 1e-4f);
    CHECK_NEAR(first, int(0.020f * kSr), 2);
}

TEST(large_and_odd_block_sizes_match) {
    std::vector<float> ref;
    for (int block : { 4096, 17, 5000 }) {
        auto e = makeEngine(kSr);
        const int frames = 20000;
        auto s = mono(renderEvents(*e, { hit(100, PadSnare, 0.8f), hit(7000, PadKick, 0.9f) }, frames, block));
        if (ref.empty()) ref = s;
        float maxDiff = 0.0f;
        for (int i = 0; i < frames; ++i) maxDiff = std::max(maxDiff, std::fabs(s[size_t(i)] - ref[size_t(i)]));
        CHECK_MSG(maxDiff < 1e-4f, "block %d differs by %g", block, double(maxDiff));
    }
}
