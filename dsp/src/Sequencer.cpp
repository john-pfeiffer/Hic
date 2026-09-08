#include "hic/seq/Sequencer.h"
#include "hic/Rng.h"
#include "hic/Math.h"
#include <cmath>

namespace hic {

double Sequencer::fireBeats(const Pattern& p, int64_t absStep) {
    const double stepBeats = 1.0 / static_cast<double>(p.stepsPerBeat < 1 ? 1 : p.stepsPerBeat);
    double beats = static_cast<double>(absStep) * stepBeats;
    if ((absStep & 1) != 0) {
        const double swing = clamp(static_cast<double>(p.swingPct), 50.0, 75.0);
        beats += (swing / 100.0 * 2.0 - 1.0) * stepBeats;     // 66.7 % = triplet feel, 75 % = max
    }
    return beats;   // the step's own nudge is added by the caller, which knows the tempo
}

int Sequencer::stepAt(const Pattern& p, int track, double ppq) {
    const Track& t = p.tracks[track];
    const int len = t.length < 1 ? 1 : t.length;
    const int64_t abs = static_cast<int64_t>(std::floor(ppq * static_cast<double>(p.stepsPerBeat) + 1e-9));
    return static_cast<int>(((abs % len) + len) % len);
}

bool Sequencer::bedGateAt(const Pattern& p, double ppq) {
    for (int k = 0; k < kNumPads; ++k) {
        const Track& t = p.tracks[k];
        if (t.mute) continue;
        const Step& s = t.steps[stepAt(p, k, ppq)];
        if (s.on && (s.flags & StepBedGate)) return true;
    }
    return false;
}

int Sequencer::process(const Pattern& p, const KitParams& kit, const Clock& clock, int n, uint32_t seed, NoteEvent* out, int max) {
    if (!clock.playing() || clock.ppqPerSample() <= 0.0) return 0;
    if (clock.discontinuity()) reset();

    const double spb       = static_cast<double>(p.stepsPerBeat < 1 ? 1 : p.stepsPerBeat);
    const double stepBeats = 1.0 / spb;
    const double beatsPerSec = clock.bpm() / 60.0;
    const double start = clock.ppqStart();
    const double end   = clock.ppqAt(n);
    // Membership is decided on integer sample positions so that a hit exactly on a
    // block boundary belongs to exactly one block whatever the block size.
    const double samplesPerBeat = clock.samplesPerBeat();
    const int64_t blockStart = static_cast<int64_t>(std::llround(start * samplesPerBeat));
    const int64_t blockEnd = blockStart + n;
    // A step can move earlier by its nudge (20 ms) or a reverse swell (up to 250 ms),
    // and later by swing (half a step) plus nudge. Scan generously; it is cheap.
    const double earlyBeats = (0.020 + 0.260) * beatsPerSec;
    const double lateBeats  = 0.5 * stepBeats + 0.020 * beatsPerSec;

    int count = 0;
    for (int k = 0; k < kNumPads && count < max; ++k) {
        const Track& t = p.tracks[k];
        if (t.mute || t.pad >= kNumPads) continue;
        const int len = t.length < 1 ? 1 : t.length;
        const PadParams& pad = kit.pads[t.pad];

        const int64_t first = static_cast<int64_t>(std::floor((start - lateBeats) * spb));
        const int64_t last  = static_cast<int64_t>(std::floor((end + earlyBeats) * spb));
        for (int64_t abs = first; abs <= last && count < max; ++abs) {
            if (abs < 0 || abs * 4 + 3 <= lastEmitted_[k]) continue;   // guard is keyed per sub-hit: abs * 4 + r
            const int idx = static_cast<int>(abs % len);
            const Step& s = t.steps[idx];
            if (!s.on) { continue; }

            double fire = fireBeats(p, abs) + static_cast<double>(s.nudgeMs) * 0.001 * beatsPerSec;
            const bool reverse = (s.flags & StepReverse) != 0 || (pad.flags & PadReverse) != 0;
            if (reverse) fire -= static_cast<double>(pad.reverseMs) * 0.001 * beatsPerSec;

            const int sub = s.ratchet >= 2 ? (s.ratchet > 4 ? 4 : s.ratchet) : 1;
            const uint32_t loop = static_cast<uint32_t>(abs / len);
            float vel = static_cast<float>(s.vel) * ((s.flags & StepAccent) ? 1.25f : 1.0f);
            for (int r = 0; r < sub && count < max; ++r, vel *= 0.85f) {
                const int64_t key = abs * 4 + r;
                if (key <= lastEmitted_[k]) continue;
                const double at = fire + static_cast<double>(r) * stepBeats / static_cast<double>(sub);
                const int64_t fireSample = static_cast<int64_t>(std::llround(at * samplesPerBeat));
                if (fireSample < blockStart || fireSample >= blockEnd) continue;   // not this block

                lastEmitted_[k] = key;
                // One roll per step, shared by its sub-hits.
                Rng dice(hashSeed(seed, static_cast<uint32_t>(abs), static_cast<uint32_t>(k) + 0x100u));
                if (s.prob < 100 && dice.uniform() * 100.0f >= static_cast<float>(s.prob)) continue;

                NoteEvent e;
                e.sampleTime = fireSample - blockStart;
                e.pad    = t.pad;
                e.note   = static_cast<uint8_t>(clamp(static_cast<int>(pad.baseNote) + s.noteOffset, 0, 127));
                e.vel    = static_cast<uint8_t>(clamp(vel, 1.0f, 127.0f) + 0.5f);
                e.source = static_cast<uint8_t>(EventSource::Seq);
                e.flags  = static_cast<uint8_t>((reverse ? EvReverse : 0) | ((s.flags & StepAccent) ? EvAccent : 0));
                e.step   = static_cast<uint16_t>(idx);
                e.bar    = loop;
                out[count++] = e;
            }
        }
    }
    return count;
}

} // namespace hic
