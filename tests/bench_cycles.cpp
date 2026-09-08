// Cycles-per-sample estimates for the unified voice and the whole engine.
// Desktop numbers are only a proxy for the Cortex-M7 target; the point is
// to notice regressions early and keep the structure lean.
#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"
#include "hic/UnifiedVoice.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;
static constexpr double kAssumedGHz = 3.0;   // for the cycles column only

static double nsPerSample(Engine& e, const std::vector<NoteEvent>& ev, int frames) {
    Timer t;
    renderEvents(e, ev, frames, 256);
    return t.seconds() * 1e9 / frames;
}

static double voiceNsPerSample(const PadParams& p, float texture, int frames) {
    KeyParams key; UnifiedVoice v; v.prepare(kSr);
    std::vector<float> buf((size_t)frames);
    Timer t;
    int hit = 0;
    for (int pos = 0; pos < frames; pos += 256) {
        if (pos % 4864 == 0) v.trigger(p, 0.9f, 60, uint32_t(++hit), key, 0.0f, texture);
        v.render(buf.data() + pos, std::min(256, frames - pos));
    }
    return t.seconds() * 1e9 / frames;
}

TEST(bench_voice_and_engine) {
    const int frames = int(kSr * 2.0f);
    std::printf("  %-22s %8s %8s\n", "case", "ns/samp", "cyc/samp");
    PadParams worst;
    worst.macro[MacroTune] = hzToTune(120.0f); worst.macro[MacroDecay] = msToDecay(600.0f);
    worst.macro[MacroExciter] = 1.0f; worst.macro[MacroBody] = 0.35f; worst.macro[MacroBreak] = 1.0f;
    const double worstNs = voiceNsPerSample(worst, 1.0f, frames);
    std::printf("  %-22s %8.1f %8.0f\n", "voice worst case", worstNs, worstNs * kAssumedGHz);
    KitParams neon; makeDefaultKit(neon);
    const double snareNs = voiceNsPerSample(neon.pads[PadSnare], 0.35f, frames);
    std::printf("  %-22s %8.1f %8.0f\n", "voice neon snare", snareNs, snareNs * kAssumedGHz);

    double engineCycles = 0.0;
    for (KitId id : { KitNeon, KitMicro, KitModular }) {
        auto e = makeFullEngine(kSr, id);
        std::vector<NoteEvent> ev;
        for (int i = 0; i < 200; ++i) ev.push_back(hit(int64_t(i) * 480, i % kNumPads, 0.9f, 60 + (i % 12)));
        const double ns = nsPerSample(*e, ev, frames);
        engineCycles = std::max(engineCycles, ns * kAssumedGHz);
        std::printf("  %-22s %8.1f %8.0f  (dense, all pads, full bus)\n", kitName(id), ns, ns * kAssumedGHz);
    }
    std::printf("  sizeof(Engine) = %.2f MB\n", double(sizeof(Engine)) / (1024.0 * 1024.0));
    CHECK_MSG(worstNs * kAssumedGHz < 400.0, "worst-case voice %.0f cycles/sample", worstNs * kAssumedGHz);
    CHECK_MSG(snareNs * kAssumedGHz < 200.0, "snare voice %.0f cycles/sample", snareNs * kAssumedGHz);
    // Desktop proxy for the Cortex-M7 budget (measure on the board before trusting it).
    CHECK_MSG(engineCycles < 3600.0, "engine cycles/sample %.0f exceeds budget", engineCycles);
    CHECK(sizeof(Engine) < 8u * 1024u * 1024u);
}
