#include "hic/Kit.h"

namespace hic {

static const char* const kPadNames[kNumPads] = {
    "Kick", "Thumb", "Side Stick", "Snare", "Paper", "Closed Hat",
    "Pedal Hat", "Open Hat", "Ride", "Woodblock", "Shaker", "Glock"
};

const char* defaultPadName(int pad) { return (pad >= 0 && pad < kNumPads) ? kPadNames[pad] : ""; }
const char* kitName(KitId id) { return id == KitMicro ? "Micro" : (id == KitModular ? "Modular" : "Neon"); }

void makeKit(KitId id, KitParams& kit) {
    if (id == KitMicro) makeMicroKit(kit);
    else if (id == KitModular) makeModularKit(kit);
    else makeDefaultKit(kit);
}

void buildNoteMap(KitParams& kit, const uint8_t* notes, const uint8_t* pads, int count) {
    for (int n = 0; n < 128; ++n) {
        int best = -1, bestDist = 1000;
        for (int i = 0; i < count; ++i) {
            const int d = n > notes[i] ? n - notes[i] : notes[i] - n;
            if (d < bestDist || (d == bestDist && notes[i] < notes[best])) { bestDist = d; best = i; }
        }
        kit.noteToPad[n] = best >= 0 ? pads[best] : 0;
    }
}

static void buildDefaultNoteMap(KitParams& kit) {
    static const uint8_t notes[] = { 35, 36, 41, 43, 45, 47, 48, 50, 37, 38, 40, 39, 42, 44, 46, 49, 57,
                                     51, 53, 59, 56, 75, 76, 77, 54, 69, 70, 82,
                                     60, 62, 64, 65, 67, 71, 72, 74, 76, 79, 81, 84 };
    static const uint8_t pads[]  = { PadKick, PadKick, PadThumb, PadThumb, PadThumb, PadThumb, PadThumb, PadThumb,
                                     PadSideStick, PadSnare, PadSnare, PadPaper, PadClosedHat, PadPedalHat, PadOpenHat, PadOpenHat, PadOpenHat,
                                     PadRide, PadRide, PadRide, PadWoodblock, PadWoodblock, PadWoodblock, PadWoodblock,
                                     PadShaker, PadShaker, PadShaker, PadShaker,
                                     PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock, PadGlock };
    buildNoteMap(kit, notes, pads, static_cast<int>(sizeof(notes) / sizeof(notes[0])));
    for (int n = 60; n < 128; ++n) {
        bool claimed = false;
        for (unsigned i = 0; i < sizeof(notes); ++i) if (notes[i] == n && pads[i] != PadGlock) claimed = true;
        if (!claimed) kit.noteToPad[n] = PadGlock;
    }
}

/// One row of a kit table: Tune in Hz, Decay in ms, then the four 0..1 macros, key follow and reverb send.
struct Row { float hz, ms, exciter, body, brk, drift; bool key; float send; };

static void applyCommon(KitParams& kit) {
    PadParams* p = kit.pads;
    p[PadKick].maxPoly = 1; p[PadKick].flags |= PadDuckSource; p[PadKick].scatterMul = 0.5f;
    p[PadThumb].flags |= PadFollowsNote; p[PadThumb].baseNote = 45;
    p[PadClosedHat].chokeGroup = 1; p[PadClosedHat].flags |= PadFreezeSource; p[PadClosedHat].scatterMul = 1.5f;
    p[PadPedalHat].chokeGroup = 1;
    p[PadOpenHat].chokeGroup = 1; p[PadOpenHat].maxPoly = 1; p[PadOpenHat].scatterMul = 1.5f;
    p[PadRide].flags |= PadFreezeSource; p[PadRide].scatterMul = 1.5f;
    p[PadShaker].scatterMul = 2.0f;
    p[PadGlock].flags |= PadFollowsNote; p[PadGlock].baseNote = 72; p[PadGlock].maxPoly = 4;
    buildDefaultNoteMap(kit);
}

static void applyRows(KitParams& kit, const Row* rows, const float* levels) {
    for (int i = 0; i < kNumPads; ++i) {
        PadParams& p = kit.pads[i];
        p = PadParams{};
        p.macro[MacroTune]    = hzToTune(rows[i].hz);
        p.macro[MacroDecay]   = msToDecay(rows[i].ms);
        p.macro[MacroExciter] = rows[i].exciter;
        p.macro[MacroBody]    = rows[i].body;
        p.macro[MacroBreak]   = rows[i].brk;
        p.macro[MacroDrift]   = rows[i].drift;
        p.flags = rows[i].key ? PadFollowKey : 0;
        p.reverbSend = rows[i].send;
        p.level = levels[i];
    }
    applyCommon(kit);
}

void makeDefaultKit(KitParams& kit) {
    static const Row rows[kNumPads] = {
        {   52.0f,  260.0f, 0.35f, 0.05f, 0.15f, 0.05f, true,  0.00f },   // kick
        {   41.0f,  180.0f, 0.40f, 0.12f, 0.10f, 0.08f, true,  0.00f },   // thumb
        {  900.0f,   45.0f, 0.05f, 0.60f, 0.20f, 0.10f, false, 0.15f },   // side stick
        {  330.0f,   70.0f, 0.30f, 0.55f, 0.35f, 0.08f, true,  0.30f },   // snare (rim)
        { 1500.0f,   60.0f, 0.85f, 0.90f, 0.25f, 0.30f, false, 0.25f },   // paper
        { 2400.0f,   12.0f, 0.15f, 0.80f, 0.10f, 0.25f, false, 0.00f },   // closed hat (CD skip)
        { 3200.0f,   30.0f, 0.80f, 0.85f, 0.05f, 0.15f, false, 0.00f },   // pedal hat (brush)
        { 3000.0f,  260.0f, 0.85f, 0.92f, 0.10f, 0.10f, false, 0.00f },   // open hat
        { 3000.0f,   80.0f, 0.95f, 0.95f, 0.45f, 0.20f, false, 0.00f },   // ride (burst)
        {  800.0f,   60.0f, 0.20f, 0.50f, 0.15f, 0.10f, true,  0.20f },   // woodblock
        { 2600.0f,   55.0f, 1.00f, 0.97f, 0.30f, 0.35f, false, 0.00f },   // shaker
        { 1046.0f,  900.0f, 0.10f, 0.62f, 0.05f, 0.05f, true,  0.35f },   // glock (tick)
    };
    static const float levels[kNumPads] = { 0.9f, 0.8f, 0.7f, 0.8f, 0.8f, 0.7f, 0.6f, 0.55f, 0.55f, 0.7f, 0.6f, 0.6f };
    applyRows(kit, rows, levels);
    kit.seed = 1;
    kit.bus = BusParams{ 0.10f, 0.35f, 0.35f, 0.30f, 1.0f, 0.5f };
}

void makeMicroKit(KitParams& kit) {
    static const Row rows[kNumPads] = {
        {   48.0f,  320.0f, 0.50f, 0.20f, 0.42f, 0.15f, true,  0.00f },
        {   55.0f,  200.0f, 0.45f, 0.30f, 0.60f, 0.25f, true,  0.00f },
        { 1200.0f,   25.0f, 0.10f, 0.70f, 0.40f, 0.40f, false, 0.20f },
        {  420.0f,   90.0f, 0.60f, 0.75f, 0.70f, 0.30f, true,  0.30f },
        { 1800.0f,   50.0f, 0.90f, 0.85f, 0.60f, 0.35f, false, 0.20f },
        { 2800.0f,   18.0f, 0.25f, 0.90f, 0.50f, 0.35f, false, 0.00f },
        { 1400.0f,   30.0f, 0.50f, 0.95f, 0.65f, 0.25f, false, 0.00f },
        { 2400.0f,  400.0f, 0.80f, 0.97f, 0.45f, 0.15f, false, 0.00f },
        { 1600.0f,  220.0f, 0.30f, 0.88f, 0.35f, 0.30f, false, 0.30f },
        {  640.0f,   70.0f, 0.35f, 0.80f, 0.80f, 0.40f, true,  0.25f },
        { 3000.0f,   60.0f, 1.00f, 1.00f, 0.70f, 0.35f, false, 0.00f },
        {  880.0f, 1200.0f, 0.20f, 0.80f, 0.20f, 0.20f, true,  0.40f },
    };
    static const float levels[kNumPads] = { 0.7f, 0.6f, 0.55f, 0.6f, 0.55f, 0.5f, 0.45f, 0.4f, 0.4f, 0.5f, 0.45f, 0.45f };
    applyRows(kit, rows, levels);
    kit.seed = 2;
    kit.bus = BusParams{ 0.25f, 0.10f, 0.45f, 0.40f, 1.3f, 0.6f };
}

void makeModularKit(KitParams& kit) {
    static const Row rows[kNumPads] = {
        {   45.0f,  400.0f, 0.55f, 0.10f, 0.62f, 0.20f, true,  0.00f },
        {   62.0f,  300.0f, 0.50f, 0.25f, 0.70f, 0.30f, true,  0.00f },
        { 1000.0f,   40.0f, 0.15f, 0.65f, 0.60f, 0.50f, false, 0.20f },
        {  260.0f,  140.0f, 0.50f, 0.60f, 0.80f, 0.35f, true,  0.30f },
        { 1200.0f,   80.0f, 0.90f, 0.80f, 0.75f, 0.40f, false, 0.20f },
        { 3000.0f,   15.0f, 0.30f, 0.95f, 0.55f, 0.45f, false, 0.00f },
        { 2000.0f,   25.0f, 0.60f, 0.90f, 0.70f, 0.30f, false, 0.00f },
        { 2600.0f,  500.0f, 0.85f, 1.00f, 0.60f, 0.20f, false, 0.00f },
        { 1900.0f,  300.0f, 0.40f, 0.90f, 0.70f, 0.30f, false, 0.30f },
        {  520.0f,  120.0f, 0.30f, 0.70f, 0.95f, 0.50f, true,  0.25f },
        { 2800.0f,   70.0f, 1.00f, 0.98f, 0.80f, 0.40f, false, 0.00f },
        {  660.0f, 1500.0f, 0.20f, 0.75f, 0.50f, 0.25f, true,  0.40f },
    };
    static const float levels[kNumPads] = { 0.6f, 0.5f, 0.5f, 0.5f, 0.5f, 0.45f, 0.4f, 0.35f, 0.35f, 0.45f, 0.4f, 0.4f };
    applyRows(kit, rows, levels);
    kit.seed = 3;
    kit.bus = BusParams{ 0.35f, 0.15f, 0.40f, 0.50f, 1.6f, 0.4f };
}

} // namespace hic
