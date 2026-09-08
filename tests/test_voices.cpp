#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"
#include <utility>

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

struct Window { int pad; float minMs, maxMs; };

TEST(every_pad_is_clean_and_in_its_decay_window) {
    auto e = makeEngine(kSr);
    const Window windows[] = {
        { PadKick,      120.0f,  500.0f }, { PadThumb,     60.0f,  500.0f },
        { PadSideStick,   5.0f,  120.0f }, { PadSnare,     30.0f,  100.0f },
        { PadPaper,       5.0f,  120.0f }, { PadClosedHat,  0.5f,   40.0f },
        { PadPedalHat,    3.0f,   60.0f }, { PadOpenHat,   60.0f,  400.0f },
        { PadRide,        5.0f,   80.0f }, { PadWoodblock,  5.0f,  200.0f },
        { PadShaker,      5.0f,  200.0f }, { PadGlock,    600.0f, 5000.0f },
    };
    for (const Window& w : windows) {
        for (float vel : { 0.3f, 0.7f, 1.0f }) {
            const int frames = int(kSr * 3.0f);
            const int note = w.pad == PadGlock ? 72 : 36;
            Stereo s = renderEvents(*e, { hit(0, w.pad, vel, note) }, frames);
            auto m = mono(s);
            const float pk = peak(m.data(), frames);
            const float dec = decayMs(m.data(), frames, kSr);
            CHECK_MSG(!hasNaN(s.l.data(), frames) && !hasNaN(s.r.data(), frames), "pad %d NaN", w.pad);
            CHECK_MSG(pk > 0.01f, "pad %d silent at vel %.1f", w.pad, double(vel));
            CHECK_MSG(pk <= dbToGain(-0.5f), "pad %d peak %.2f dBFS", w.pad, double(gainToDb(pk)));
            CHECK_MSG(dec >= w.minMs && dec <= w.maxMs, "pad %d (%s) vel %.1f decay %.1f ms not in [%g, %g]",
                      w.pad, defaultPadName(w.pad), double(vel), double(dec), double(w.minMs), double(w.maxMs));
            // Once the hit is over the voice must free itself.
            CHECK_MSG(e->activeVoices() == 0 || w.pad == PadGlock, "pad %d still active after 3 s", w.pad);
        }
    }
}

TEST(velocity_changes_level) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr);
    for (int pad = 0; pad < kNumPads; ++pad) {
        auto soft = mono(renderEvents(*e, { hit(0, pad, 0.3f, 72) }, frames));
        auto loud = mono(renderEvents(*e, { hit(0, pad, 1.0f, 72) }, frames));
        CHECK_MSG(peak(loud.data(), frames) > peak(soft.data(), frames) * 1.3f, "pad %d velocity has no effect", pad);
    }
}

TEST(kick_has_no_top_end_and_hats_have_no_bottom) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr);
    auto kick = mono(renderEvents(*e, { hit(0, PadKick, 0.9f) }, frames));
    CHECK_MSG(bandRatioDb(kick.data(), frames, kSr, 4000.0f) < -30.0f, "kick top end %.1f dB", double(bandRatioDb(kick.data(), frames, kSr, 4000.0f)));
    auto thumb = mono(renderEvents(*e, { hit(0, PadThumb, 0.9f, 45) }, frames));
    CHECK(bandRatioDb(thumb.data(), frames, kSr, 4000.0f) < -30.0f);
    // The open hat is clean filtered noise; the pedal hat is a 12-bit brush whose
    // quantization floor is broadband by design, so it gets a looser bound.
    for (auto pr : { std::pair<int, float>{ PadOpenHat, 20.0f }, std::pair<int, float>{ PadPedalHat, 12.0f } }) {
        auto h = mono(renderEvents(*e, { hit(0, pr.first, 0.9f) }, frames));
        const float r = bandRatioDb(h.data(), frames, kSr, 2000.0f);
        CHECK_MSG(r > pr.second, "pad %d bottom end ratio %.1f dB", pr.first, double(r));
    }
    auto ch = mono(renderEvents(*e, { hit(0, PadClosedHat, 0.9f) }, frames));
    CHECK(bandRatioDb(ch.data(), frames, kSr, 2000.0f) > 6.0f);
}

TEST(tail_cut_ends_the_snare) {
    auto e = makeEngine(kSr);
    e->kit.pads[PadSnare].tailCutMs = 40.0f;
    const int frames = int(kSr);
    auto s = mono(renderEvents(*e, { hit(0, PadSnare, 0.9f) }, frames));
    const int last = lastIndexAbove(s.data(), frames, 1e-4f);
    CHECK_MSG(last <= int((40.0f + 2.0f) * 0.001f * kSr), "snare rings until %.1f ms", double(last) * 1000.0 / double(kSr));
}

TEST(glock_follows_the_note) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr * 0.5f);
    auto lo = mono(renderEvents(*e, { hit(0, PadGlock, 0.9f, 60) }, frames));
    auto hi = mono(renderEvents(*e, { hit(0, PadGlock, 0.9f, 72) }, frames));
    // Count zero crossings over the sustained part: the octave should double them.
    auto crossings = [&](const std::vector<float>& x) { int c = 0; for (int i = 4801; i < frames; ++i) if ((x[size_t(i)] >= 0) != (x[size_t(i - 1)] >= 0)) ++c; return c; };
    const int cl = crossings(lo), ch = crossings(hi);
    CHECK_MSG(float(ch) > float(cl) * 1.6f && float(ch) < float(cl) * 2.4f, "crossings lo %d hi %d", cl, ch);
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

TEST(modal_presets_all_render) {
    auto e = makeEngine(kSr);
    const int frames = int(kSr * 2.0f);
    for (int ps = 0; ps < ModalPresetCount; ++ps) {
        e->kit.pads[PadWoodblock].preset = uint8_t(ps);
        e->kit.pads[PadWoodblock].flags = modalPreset(ps).followsNote ? PadFollowsNote : 0;
        e->kit.pads[PadWoodblock].lowpassHz = 20000.0f;
        auto s = mono(renderEvents(*e, { hit(0, PadWoodblock, 0.9f, 67) }, frames));
        CHECK_MSG(!hasNaN(s.data(), frames), "modal preset %d NaN", ps);
        CHECK_MSG(peak(s.data(), frames) > 0.02f && peak(s.data(), frames) <= 1.0f, "modal preset %s peak %.3f", modalPreset(ps).name, double(peak(s.data(), frames)));
    }
}
