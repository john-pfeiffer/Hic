// Renders every pad, a General MIDI demo beat and (later) a sequencer demo
// to WAV files so the sounds can be listened to and inspected.
#include <cstdio>
#include <string>
#include "Render.h"
#include "WavWriter.h"
#include "hic/Kit.h"
#include "hic/seq/Pattern.h"

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

static void writeMono(const std::string& path, const std::vector<float>& m) { writeWav(path.c_str(), m.data(), nullptr, int(m.size()), int(kSr)); }
static void writeStereo(const std::string& path, const Stereo& s) { writeWav(path.c_str(), s.l.data(), s.r.data(), int(s.l.size()), int(kSr)); }

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "render";
    std::printf("Rendering to %s/\n", dir.c_str());

    auto e = makeEngine(kSr);
    for (int pad = 0; pad < kNumPads; ++pad) {
        const int frames = int(kSr * (pad == PadGlock ? 3.0f : 1.5f));
        const int note = pad == PadGlock ? 72 : (pad == PadThumb ? 45 : 36);
        auto m = mono(renderEvents(*e, { hit(0, pad, 0.9f, note) }, frames));
        char name[64]; std::snprintf(name, sizeof name, "pad_%02d_%s.wav", pad, defaultPadName(pad));
        for (char* c = name; *c; ++c) if (*c == ' ') *c = '_';
        writeMono(dir + "/" + name, m);
        printStats(name, m.data(), frames, kSr);
    }

    // The Micro kit: every pad, and the GM beat again through it.
    auto micro = makeEngine(kSr);
    makeMicroKit(micro->kit);
    for (int pad = 0; pad < kNumPads; ++pad) {
        const int frames = int(kSr * (pad == PadGlock ? 3.0f : 1.5f));
        const int note = pad == PadGlock ? 72 : (pad == PadThumb ? 45 : 36);
        auto m = mono(renderEvents(*micro, { hit(0, pad, 0.9f, note) }, frames));
        char name[64]; std::snprintf(name, sizeof name, "micro_%02d_%s.wav", pad, defaultPadName(pad));
        for (char* c = name; *c; ++c) if (*c == ' ') *c = '_';
        writeMono(dir + "/" + name, m);
        printStats(name, m.data(), frames, kSr);
    }

    // A two-bar GM beat at 92 BPM, twice, straight from note numbers.
    {
        const double bpm = 92.0;
        const double step = 60.0 / bpm / 4.0;   // 16th
        struct S { int step; int note; float vel; };
        const S beat[] = {
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
        for (int loop = 0; loop < 2; ++loop)
            for (const S& s : beat)
                ev.push_back(midiHit(int64_t(double(loop * 32 + s.step) * step * double(kSr)), s.note, s.vel));
        const int frames = int(64.0 * step * double(kSr)) + int(kSr);
        {
            auto dry = makeEngine(kSr);
            Stereo s = renderEvents(*dry, ev, frames);
            writeStereo(dir + "/gm_pattern_dry.wav", s);
            auto m = mono(s);
            printStats("gm_pattern_dry.wav", m.data(), frames, kSr);
        }
        {
            // The instrument as shipped: feel, crackle bed ducked to the kick, spring, rare stutter.
            auto full = makeFullEngine(kSr);
            full->feel.lookaheadMs = 20.0f; full->prepare(kSr);
            full->repeat.probability = 0.5f;   // a little more eager than the default so the demo shows one
            Stereo s = renderEvents(*full, ev, frames, 256, bpm);
            writeStereo(dir + "/gm_pattern.wav", s);
            auto m = mono(s);
            printStats("gm_pattern.wav", m.data(), frames, kSr);
            std::printf("  (stutters in gm_pattern: %d)\n", full->beatRepeat().repeatCount());
        }
        {
            auto mk = makeFullEngine(kSr);
            makeMicroKit(mk->kit);
            mk->feel.lookaheadMs = 20.0f; mk->prepare(kSr);
            Stereo s = renderEvents(*mk, ev, frames, 256, bpm);
            writeStereo(dir + "/gm_pattern_micro.wav", s);
            auto m = mono(s);
            printStats("gm_pattern_micro.wav", m.data(), frames, kSr);
        }
        {
            // Bed only: crackle and hiss, gated to eighths, ducked by a silent kick.
            auto b = makeFullEngine(kSr);
            b->bed.type = BedType::Both; b->bed.level = 0.5f; b->bed.gate = BedGate::Eighths; b->bed.gateDuty = 0.6f;
            b->kit.pads[PadKick].level = 0.0f;
            std::vector<NoteEvent> kicks;
            for (int i = 0; i < 16; ++i) kicks.push_back(hit(int64_t(double(i * 4) * step * double(kSr)), PadKick, 1.0f));
            const int bframes = int(kSr * 8.0f);
            Stereo s = renderEvents(*b, kicks, bframes, 256, bpm);
            writeStereo(dir + "/bed_crackle_ducked.wav", s);
            auto m = mono(s);
            printStats("bed_crackle_ducked.wav", m.data(), bframes, kSr);
        }
        {
            // Freeze: two clicks, then hold the freeze for two seconds of insect ticking.
            auto f = makeFullEngine(kSr);
            f->bed.type = BedType::Off; f->freeze.mix = 0.8f; f->freeze.grainMs = 8.0f; f->freeze.density = 18.0f; f->freeze.sprayMs = 40.0f;
            NoteEvent on = midiHit(int(0.6f * kSr), kNoteFreezeHold, 1.0f);
            NoteEvent off = midiHit(int(2.8f * kSr), kNoteFreezeHold, 1.0f); off.flags = EvNoteOff;
            const int fframes = int(kSr * 3.5f);
            Stereo s = renderEvents(*f, { hit(int(0.2f * kSr), PadClosedHat, 0.9f), hit(int(0.4f * kSr), PadRide, 0.8f), on, off }, fframes, 256, bpm);
            writeStereo(dir + "/fx_freeze_ticks.wav", s);
            auto m = mono(s);
            printStats("fx_freeze_ticks.wav", m.data(), fframes, kSr);
        }
    }
    // The internal sequencer: eight bars of the demo pattern on the internal clock.
    {
        auto sq = makeFullEngine(kSr);
        makeDemoPattern(sq->patterns[0]);
        sq->feel.lookaheadMs = 20.0f; sq->prepare(kSr);
        sq->global.internalPlay = true; sq->global.internalBpm = 88.0;
        sq->bed.gate = BedGate::Steps; sq->bed.level = 0.3f;
        const int frames = int(8.0 * 4.0 * 60.0 / 88.0 * double(kSr)) + int(kSr);
        Stereo s = renderEvents(*sq, {}, frames, 256);
        writeStereo(dir + "/seq_demo.wav", s);
        auto m = mono(s);
        printStats("seq_demo.wav", m.data(), frames, kSr);
        std::printf("  (stutters in seq_demo: %d)\n", sq->beatRepeat().repeatCount());
    }
    return 0;
}
