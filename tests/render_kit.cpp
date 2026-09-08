// Renders every pad of every kit, GM demo beats, macro sweeps, the static
// pulses, a freeze demo and the sequencer demo to WAV files so the sounds
// can be listened to and inspected.
#include <cstdio>
#include <string>
#include <cctype>
#include "Render.h"
#include "WavWriter.h"
#include "hic/Kit.h"
#include "hic/seq/Pattern.h"
#include "hic/UnifiedVoice.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

static void writeMono(const std::string& path, const std::vector<float>& m) { writeWav(path.c_str(), m.data(), nullptr, int(m.size()), int(kSr)); }
static void writeStereo(const std::string& path, const Stereo& s) { writeWav(path.c_str(), s.l.data(), s.r.data(), int(s.l.size()), int(kSr)); }
static int auditionNote(int pad) { return pad == PadGlock ? 72 : (pad == PadThumb ? 45 : 36); }

static std::vector<NoteEvent> gmBeat(double bpm, int loops) {
    const double step = 60.0 / bpm / 4.0;
    struct S { int step; int note; float vel; };
    static const S beat[] = {
        {0, 36, 1.0f}, {10, 36, 0.8f}, {16, 36, 0.95f}, {22, 36, 0.7f}, {26, 36, 0.85f},
        {4, 38, 0.9f}, {12, 38, 0.85f}, {20, 38, 0.9f}, {28, 38, 0.85f}, {30, 37, 0.5f},
        {0, 42, 0.6f}, {2, 42, 0.4f}, {4, 42, 0.6f}, {6, 42, 0.45f}, {8, 42, 0.6f}, {10, 42, 0.4f}, {12, 42, 0.6f}, {14, 46, 0.5f},
        {16, 42, 0.6f}, {18, 42, 0.4f}, {20, 42, 0.6f}, {22, 42, 0.45f}, {24, 42, 0.6f}, {26, 42, 0.4f}, {28, 42, 0.6f}, {30, 42, 0.45f},
        {7, 56, 0.5f}, {23, 56, 0.5f}, {15, 51, 0.5f}, {31, 51, 0.4f},
        {1, 70, 0.5f}, {3, 70, 0.4f}, {5, 70, 0.5f}, {7, 70, 0.4f}, {9, 70, 0.5f}, {11, 70, 0.4f}, {13, 70, 0.5f}, {15, 70, 0.4f},
        {17, 70, 0.5f}, {19, 70, 0.4f}, {21, 70, 0.5f}, {23, 70, 0.4f}, {25, 70, 0.5f}, {27, 70, 0.4f}, {29, 70, 0.5f}, {31, 70, 0.4f},
        {2, 72, 0.5f}, {11, 76, 0.45f}, {18, 79, 0.5f}, {27, 74, 0.45f},
    };
    std::vector<NoteEvent> ev;
    for (int loop = 0; loop < loops; ++loop)
        for (const S& s : beat)
            ev.push_back(midiHit(int64_t(double(loop * 32 + s.step) * step * double(kSr)), s.note, s.vel));
    return ev;
}

static void sweep(const std::string& dir, const char* name, int macro, float tuneHz, float decayMs, float exciter, float body, float brk, int steps, float stepSec) {
    KeyParams key; UnifiedVoice v; v.prepare(kSr);
    const int per = int(stepSec * kSr);
    std::vector<float> out((size_t)per * (size_t)steps, 0.0f);
    for (int s = 0; s < steps; ++s) {
        PadParams p;
        p.macro[MacroTune] = hzToTune(tuneHz); p.macro[MacroDecay] = msToDecay(decayMs);
        p.macro[MacroExciter] = exciter; p.macro[MacroBody] = body; p.macro[MacroBreak] = brk; p.macro[MacroDrift] = 0.0f;
        p.macro[macro] = float(s) / float(steps - 1);
        v.trigger(p, 0.9f, 60, uint32_t(s + 1), key, 0.0f, 0.3f);
        for (int pos = 0; pos < per; pos += 256) v.render(out.data() + size_t(s) * size_t(per) + size_t(pos), std::min(256, per - pos));
    }
    for (auto& x : out) x *= 0.7f;
    writeMono(dir + "/" + name, out);
    printStats(name, out.data(), int(out.size()), kSr);
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "render";
    std::printf("Rendering to %s/\n", dir.c_str());
    const double bpm = 92.0;
    const double step = 60.0 / bpm / 4.0;
    const int beatFrames = int(64.0 * step * double(kSr)) + int(kSr);

    for (KitId id : { KitNeon, KitMicro, KitModular }) {
        std::string kn = kitName(id);
        for (auto& c : kn) c = char(std::tolower(c));
        std::printf("-- %s\n", kitName(id));
        auto e = makeEngine(kSr);
        makeKit(id, e->kit);
        for (int pad = 0; pad < kNumPads; ++pad) {
            const int frames = int(kSr * (pad == PadGlock ? 3.0f : 1.5f));
            auto m = mono(renderEvents(*e, { hit(0, pad, 0.9f, auditionNote(pad)) }, frames));
            char name[64]; std::snprintf(name, sizeof name, "%s_%02d_%s.wav", kn.c_str(), pad, defaultPadName(pad));
            for (char* c = name; *c; ++c) if (*c == ' ') *c = '_';
            writeMono(dir + "/" + name, m);
            printStats(name, m.data(), frames, kSr);
        }
        {
            auto dry = makeEngine(kSr); makeKit(id, dry->kit);
            Stereo s = renderEvents(*dry, gmBeat(bpm, 2), beatFrames);
            writeStereo(dir + "/" + kn + "_gm_dry.wav", s);
            auto m = mono(s); printStats((kn + "_gm_dry.wav").c_str(), m.data(), beatFrames, kSr);
        }
        {
            auto full = makeFullEngine(kSr, id);
            full->feel.lookaheadMs = 20.0f; full->prepare(kSr);
            full->repeat.probability = 0.5f;
            Stereo s = renderEvents(*full, gmBeat(bpm, 2), beatFrames, 256, bpm);
            writeStereo(dir + "/" + kn + "_gm_full.wav", s);
            auto m = mono(s); printStats((kn + "_gm_full.wav").c_str(), m.data(), beatFrames, kSr);
            std::printf("  (stutters: %d)\n", full->beatRepeat().repeatCount());
        }
    }

    std::printf("-- sweeps\n");
    sweep(dir, "sweep_body.wav", MacroBody, 200.0f, 400.0f, 0.3f, 0.5f, 0.0f, 9, 0.5f);
    sweep(dir, "sweep_exciter.wav", MacroExciter, 300.0f, 150.0f, 0.5f, 0.6f, 0.0f, 9, 0.4f);
    sweep(dir, "sweep_break.wav", MacroBreak, 110.0f, 600.0f, 0.4f, 0.3f, 0.0f, 11, 1.0f);
    sweep(dir, "sweep_break_metal.wav", MacroBreak, 700.0f, 500.0f, 0.2f, 0.85f, 0.0f, 11, 0.8f);

    std::printf("-- textures\n");
    {
        auto e = makeFullEngine(kSr);
        e->bus.texture = 1.0f; e->statik.levelDetail = 0.8f; e->statik.clock = StaticClock::Sixteenths; e->statik.colour = 0.4f;
        e->kit.pads[PadKick].level = 0.0f;
        std::vector<NoteEvent> kicks;
        for (int i = 0; i < 16; ++i) kicks.push_back(hit(int64_t(double(i * 4) * step * double(kSr)), PadKick, 1.0f));
        const int frames = int(kSr * 8.0f);
        Stereo s = renderEvents(*e, kicks, frames, 256, bpm);
        writeStereo(dir + "/static_pulses.wav", s);
        auto m = mono(s); printStats("static_pulses.wav", m.data(), frames, kSr);
    }
    {
        auto f = makeFullEngine(kSr);
        f->bus.texture = 0.0f; f->freeze.mix = 0.8f; f->freeze.grainMs = 8.0f; f->freeze.density = 18.0f; f->freeze.sprayMs = 40.0f;
        NoteEvent on = midiHit(int(0.6f * kSr), kNoteFreezeHold, 1.0f);
        NoteEvent off = midiHit(int(2.8f * kSr), kNoteFreezeHold, 1.0f); off.flags = EvNoteOff;
        const int frames = int(kSr * 3.5f);
        Stereo s = renderEvents(*f, { hit(int(0.2f * kSr), PadClosedHat, 0.9f), hit(int(0.4f * kSr), PadRide, 0.8f), on, off }, frames, 256, bpm);
        writeStereo(dir + "/fx_freeze_ticks.wav", s);
        auto m = mono(s); printStats("fx_freeze_ticks.wav", m.data(), frames, kSr);
    }
    {
        auto sq = makeFullEngine(kSr);
        makeDemoPattern(sq->patterns[0]);
        sq->feel.lookaheadMs = 20.0f; sq->prepare(kSr);
        sq->global.internalPlay = true; sq->global.internalBpm = 88.0;
        sq->statik.clock = StaticClock::Steps;
        const int frames = int(8.0 * 4.0 * 60.0 / 88.0 * double(kSr)) + int(kSr);
        Stereo s = renderEvents(*sq, {}, frames, 256);
        writeStereo(dir + "/seq_demo.wav", s);
        auto m = mono(s); printStats("seq_demo.wav", m.data(), frames, kSr);
        std::printf("  (stutters: %d)\n", sq->beatRepeat().repeatCount());
    }
    return 0;
}
