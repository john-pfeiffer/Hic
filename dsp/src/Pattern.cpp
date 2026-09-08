#include "hic/seq/Pattern.h"
#include "hic/Kit.h"

namespace hic {

void clearPattern(Pattern& p) {
    p = Pattern{};
    for (int k = 0; k < kNumPads; ++k) { p.tracks[k].pad = static_cast<uint8_t>(k); p.tracks[k].length = 16; }
}

static Step& at(Pattern& p, int track, int step) { return p.tracks[track].steps[step]; }
static void put(Pattern& p, int track, int step, int vel, int prob = 100, int nudge = 0, uint8_t flags = 0, int note = 0, int ratchet = 0) {
    Step& s = at(p, track, step);
    s.on = 1; s.vel = static_cast<uint8_t>(vel); s.prob = static_cast<uint8_t>(prob);
    s.nudgeMs = static_cast<int8_t>(nudge); s.flags = flags; s.noteOffset = static_cast<int8_t>(note);
    s.ratchet = static_cast<uint8_t>(ratchet);
}

void makeDemoPattern(Pattern& p) {
    clearPattern(p);
    p.stepsPerBeat = 4;
    p.swingPct = 54;

    // Kick: one and the "and" of three, a ghost on the last 16th sometimes.
    put(p, PadKick, 0, 118);
    put(p, PadKick, 10, 96, 100, 3);
    put(p, PadKick, 15, 60, 35, 0);

    // Thumb: an occasional low answer.
    put(p, PadThumb, 7, 70, 45, -4);

    // Backbeat: side stick on two, clipped rimshot on four.
    put(p, PadSideStick, 4, 100, 100, 2);
    put(p, PadSnare, 12, 104, 100, -2);
    put(p, PadSnare, 13, 44, 20, 0);

    // Closed hat clicks on the eighths, offbeat 16ths only sometimes, uneven.
    static const int hatVel[16] = { 84, 0, 62, 0, 78, 0, 58, 0, 80, 0, 60, 0, 76, 0, 64, 0 };
    for (int i = 0; i < 16; ++i) {
        if (hatVel[i]) put(p, PadClosedHat, i, hatVel[i], 100, (i % 4 == 2) ? 2 : 0, (i < 4 || (i >= 8 && i < 12)) ? StepBedGate : 0);
        else           put(p, PadClosedHat, i, 40, 30, -3);
    }
    at(p, PadClosedHat, 14).ratchet = 3; at(p, PadClosedHat, 14).prob = 25;

    // Pedal hat under the backbeats; one reversed open hat swelling into the downbeat.
    put(p, PadPedalHat, 4, 60, 70);
    put(p, PadPedalHat, 12, 58, 70);
    put(p, PadOpenHat, 14, 70, 40, 0, StepReverse);

    // Ride burst and paper clap as colour.
    put(p, PadRide, 6, 66, 55, 4);
    put(p, PadPaper, 12, 52, 30, 6);

    // Shaker: every 16th, velocities deliberately uneven, a little late.
    static const int shk[16] = { 58, 34, 48, 40, 60, 30, 46, 38, 56, 36, 50, 32, 62, 34, 44, 42 };
    for (int i = 0; i < 16; ++i) put(p, PadShaker, i, shk[i], 92, (i & 1) ? 5 : 1);

    // Woodblock: a polymetric 12-step tick.
    p.tracks[PadWoodblock].length = 12;
    put(p, PadWoodblock, 3, 64, 70, 0);
    put(p, PadWoodblock, 9, 58, 50, 0);

    // Glock: a 24-step melodic loop so the notes drift against the bar.
    p.tracks[PadGlock].length = 24;
    put(p, PadGlock, 2,  78, 100, 0, 0, 0);
    put(p, PadGlock, 9,  70, 100, 3, 0, 7);
    put(p, PadGlock, 14, 66, 80,  0, 0, 12);
    put(p, PadGlock, 19, 72, 100, -2, 0, 4);
    put(p, PadGlock, 22, 60, 60,  0, 0, -5);
}

} // namespace hic
