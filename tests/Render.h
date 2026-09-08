#pragma once
// Helpers to render hits through an Engine into mono/stereo buffers.
#include <vector>
#include <memory>
#include <cstdio>
#include "hic/Engine.h"
#include "Analysis.h"

namespace hictest {

struct Stereo { std::vector<float> l, r; };

inline std::unique_ptr<hic::Engine> makeEngine(float sr) {
    auto e = std::make_unique<hic::Engine>();
    hic::makeDefaultKit(e->kit);
    // Tests look at voices in isolation: no feel randomness, no bed, no reverb, no stutter.
    e->feel.scatterMs = 0.0f;
    e->feel.velScatter = 0.0f;
    e->bed.type = hic::BedType::Off;
    e->reverb.mix = 0.0f;
    e->repeat.enabled = false;
    e->prepare(sr);
    return e;
}

/// The instrument as shipped: default feel, bed, reverb and stutter.
inline std::unique_ptr<hic::Engine> makeFullEngine(float sr) {
    auto e = std::make_unique<hic::Engine>();
    hic::makeDefaultKit(e->kit);
    e->prepare(sr);
    return e;
}

/// Runs the engine for `frames` samples in blocks of `block`, feeding `events`
/// (sampleTime absolute from the start of the render).
inline Stereo renderEvents(hic::Engine& e, const std::vector<hic::NoteEvent>& events, int frames, int block = 256, double bpm = 0.0) {
    Stereo out; out.l.assign(size_t(frames), 0.0f); out.r.assign(size_t(frames), 0.0f);
    hic::TransportInfo t;
    if (bpm > 0.0) { t.valid = true; t.playing = true; t.bpm = bpm; }
    hic::NoteEvent in[hic::kMaxEvents];
    for (int pos = 0; pos < frames; pos += block) {
        const int n = (frames - pos) < block ? (frames - pos) : block;
        if (bpm > 0.0) t.ppq = bpm / 60.0 * double(pos) / double(e.sampleRate());
        int nIn = 0;
        for (const auto& ev : events)
            if (ev.sampleTime >= pos && ev.sampleTime < pos + n && nIn < hic::kMaxEvents) { in[nIn] = ev; in[nIn].sampleTime -= pos; ++nIn; }
        e.process(in, nIn, t, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

inline hic::NoteEvent hit(int64_t at, int pad, float vel, int note = 36) {
    hic::NoteEvent e; e.sampleTime = at; e.pad = uint8_t(pad); e.vel = uint8_t(vel * 127.0f + 0.5f); e.note = uint8_t(note);
    e.source = uint8_t(hic::EventSource::Internal);
    return e;
}

inline hic::NoteEvent midiHit(int64_t at, int note, float vel) {
    hic::NoteEvent e; e.sampleTime = at; e.note = uint8_t(note); e.vel = uint8_t(vel * 127.0f + 0.5f);
    e.source = uint8_t(hic::EventSource::Midi);
    return e;
}

inline std::vector<float> mono(const Stereo& s) {
    std::vector<float> m(s.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (s.l[i] + s.r[i]);
    return m;
}

inline void printStats(const char* name, const float* x, int n, float sr) {
    std::printf("  %-22s peak %6.1f dBFS  rms %6.1f dB  decay %7.1f ms  >4k/<4k %6.1f dB  NaN %s\n",
                name, double(peakDb(x, n)), double(hic::gainToDb(rms(x, n))), double(decayMs(x, n, sr)),
                double(bandRatioDb(x, n, sr, 4000.0f)), hasNaN(x, n) ? "YES" : "no");
}

} // namespace hictest
