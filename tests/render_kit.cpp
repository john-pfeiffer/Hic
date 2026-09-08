// Renders every pad, a General MIDI demo beat and (later) a sequencer demo
// to WAV files so the sounds can be listened to and inspected.
#include <cstdio>
#include <string>
#include "Render.h"
#include "WavWriter.h"
#include "hic/Kit.h"

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
        auto e2 = makeEngine(kSr);
        Stereo s = renderEvents(*e2, ev, frames);
        writeStereo(dir + "/gm_pattern.wav", s);
        auto m = mono(s);
        printStats("gm_pattern.wav", m.data(), frames, kSr);
    }
    return 0;
}
