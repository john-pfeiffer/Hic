// Cycles-per-sample estimates for each voice type and the whole engine.
// Desktop numbers are only a proxy for the Cortex-M7 target; the point is
// to notice regressions early and keep the structure lean.
#include "Harness.h"
#include "Render.h"
#include "hic/Kit.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;
static constexpr double kAssumedGHz = 3.0;   // for the cycles column only

static double nsPerSample(Engine& e, const std::vector<NoteEvent>& ev, int frames) {
    Timer t;
    renderEvents(e, ev, frames, 256);
    return t.seconds() * 1e9 / frames;
}

TEST(bench_voices_and_engine) {
    const int frames = int(kSr * 2.0f);
    std::printf("  %-14s %8s %8s\n", "case", "ns/samp", "cyc/samp");
    double engineCycles = 0.0;
    for (int pad : { PadKick, PadClosedHat, PadSnare, PadOpenHat, PadGlock }) {
        auto e = makeEngine(kSr);
        std::vector<NoteEvent> ev;
        for (int i = 0; i < 20; ++i) ev.push_back(hit(int64_t(i) * 4800, pad, 0.9f, 60 + (i % 12)));
        const double ns = nsPerSample(*e, ev, frames);
        std::printf("  %-14s %8.1f %8.0f\n", defaultPadName(pad), ns, ns * kAssumedGHz);
    }
    {
        auto e = makeEngine(kSr);
        std::vector<NoteEvent> ev;
        for (int i = 0; i < 200; ++i) ev.push_back(hit(int64_t(i) * 480, i % kNumPads, 0.9f, 60 + (i % 12)));
        const double ns = nsPerSample(*e, ev, frames);
        engineCycles = ns * kAssumedGHz;
        std::printf("  %-14s %8.1f %8.0f  (dense, all pads)\n", "engine", ns, engineCycles);
    }
    std::printf("  sizeof(Engine) = %.2f MB\n", double(sizeof(Engine)) / (1024.0 * 1024.0));
    CHECK_MSG(engineCycles < 3000.0, "engine cycles/sample %.0f exceeds budget", engineCycles);
    CHECK(sizeof(Engine) < 8u * 1024u * 1024u);
}
