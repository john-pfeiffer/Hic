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

/// Time of one reference unit: a TPT state-variable filter tick (about ten flops).
/// Budgets are expressed in these units so the test does not depend on how fast
/// the machine running it happens to be.
static double referenceNsPerTick(int frames) {
    TptSvf f; f.set(1000.0f, 0.7f, kSr);
    Rng r(1);
    std::vector<float> in((size_t)frames);
    for (auto& x : in) x = r.bipolar();
    double best = 1e9;
    for (int rep = 0; rep < 5; ++rep) {
        Timer t;
        float acc = 0.0f;
        for (int i = 0; i < frames; ++i) acc += f.lp(in[(size_t)i]);
        const double ns = t.seconds() * 1e9 / frames;
        if (acc == 12345.0f) std::printf("");   // keep the loop alive
        if (ns < best) best = ns;
    }
    return best;
}

TEST(bench_voice_and_engine) {
    const int frames = int(kSr * 2.0f);
    const double unit = referenceNsPerTick(frames);
    std::printf("  reference SVF tick: %.2f ns\n", unit);
    std::printf("  %-22s %8s %8s %10s\n", "case", "ns/samp", "cyc/samp", "svf-ticks");
    PadParams worst;
    worst.macro[MacroTune] = hzToTune(120.0f); worst.macro[MacroDecay] = msToDecay(600.0f);
    worst.macro[MacroExciter] = 1.0f; worst.macro[MacroBody] = 0.35f; worst.macro[MacroBreak] = 1.0f;
    const double worstNs = voiceNsPerSample(worst, 1.0f, frames);
    std::printf("  %-22s %8.1f %8.0f %10.1f\n", "voice worst case", worstNs, worstNs * kAssumedGHz, worstNs / unit);
    KitParams neon; makeDefaultKit(neon);
    const double snareNs = voiceNsPerSample(neon.pads[PadSnare], 0.35f, frames);
    std::printf("  %-22s %8.1f %8.0f %10.1f\n", "voice neon snare", snareNs, snareNs * kAssumedGHz, snareNs / unit);

    double engineNs = 0.0;
    for (KitId id : { KitNeon, KitMicro, KitModular }) {
        auto e = makeFullEngine(kSr, id);
        std::vector<NoteEvent> ev;
        for (int i = 0; i < 200; ++i) ev.push_back(hit(int64_t(i) * 480, i % kNumPads, 0.9f, 60 + (i % 12)));
        const double ns = nsPerSample(*e, ev, frames);
        engineNs = std::max(engineNs, ns);
        std::printf("  %-22s %8.1f %8.0f %10.1f  (dense, all pads, full bus)\n", kitName(id), ns, ns * kAssumedGHz, ns / unit);
    }
    std::printf("  sizeof(Engine) = %.2f MB\n", double(sizeof(Engine)) / (1024.0 * 1024.0));
    // Budgets in reference ticks, with better than double headroom for noisy runners. On this
    // design the worst voice measures about 14 ticks, a typical pad about 10, the dense engine about 200.
    CHECK_MSG(worstNs / unit < 45.0, "worst-case voice %.1f svf-ticks/sample", worstNs / unit);
    CHECK_MSG(snareNs / unit < 35.0, "snare voice %.1f svf-ticks/sample", snareNs / unit);
    CHECK_MSG(engineNs / unit < 700.0, "engine %.1f svf-ticks/sample exceeds budget", engineNs / unit);
    CHECK(sizeof(Engine) < 8u * 1024u * 1024u);
}
