#include "hic/Kit.h"
#include "hic/voices/ModalPresets.h"
#include "hic/voices/PingVoice.h"

namespace hic {

static const char* const kPadNames[kNumPads] = {
    "Kick", "Thumb", "Side Stick", "Snare", "Paper", "Closed Hat",
    "Pedal Hat", "Open Hat", "Ride", "Woodblock", "Shaker", "Glock"
};

const char* defaultPadName(int pad) { return (pad >= 0 && pad < kNumPads) ? kPadNames[pad] : ""; }

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

static void setMacros(PadParams& p, float m0, float m1, float m2, float m3, float m4, float m5, float m6, float m7) {
    p.macro[0] = m0; p.macro[1] = m1; p.macro[2] = m2; p.macro[3] = m3;
    p.macro[4] = m4; p.macro[5] = m5; p.macro[6] = m6; p.macro[7] = m7;
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
    // Everything from 60 up that is not otherwise claimed is glock, so melodic input plays notes.
    for (int n = 60; n < 128; ++n) {
        bool claimed = false;
        for (unsigned i = 0; i < sizeof(notes); ++i) if (notes[i] == n && pads[i] != PadGlock) claimed = true;
        if (!claimed) kit.noteToPad[n] = PadGlock;
    }
}

const char* kitName(KitId id) { return id == KitMicro ? "Micro" : "Neon"; }

void makeKit(KitId id, KitParams& kit) { if (id == KitMicro) makeMicroKit(kit); else makeDefaultKit(kit); }

void makeMicroKit(KitParams& kit) {
    for (int i = 0; i < kNumPads; ++i) kit.pads[i] = PadParams{};
    kit.seed = 2;
    PadParams* p = kit.pads;

    // 0 Kick: a sine drop with a click of noise, lowpassed so it stays soft.
    p[PadKick].type = PadType::Ping; p[PadKick].preset = PingSine;
    setMacros(p[PadKick], 0.12f, 0.5f, 0.78f, 0.25f, 0.3f, 0.0f, 0.3f, 0.12f);
    p[PadKick].maxPoly = 1; p[PadKick].level = 0.9f; p[PadKick].lowpassHz = 2500.0f; p[PadKick].driveDb = 5.0f;
    p[PadKick].flags = PadDuckSource; p[PadKick].scatterMul = 0.5f; p[PadKick].morph = 0.1f;

    // 1 Thumb: feedback FM thud, tracks the toms.
    p[PadThumb].type = PadType::Ping; p[PadThumb].preset = PingFeedback;
    setMacros(p[PadThumb], 0.2f, 0.42f, 0.65f, 0.3f, 0.2f, 0.55f, 0.35f, 0.1f);
    p[PadThumb].level = 0.8f; p[PadThumb].lowpassHz = 1800.0f; p[PadThumb].flags = PadFollowsNote; p[PadThumb].baseNote = 45; p[PadThumb].morph = 0.2f;

    // 2 Side stick: a dense cluster of CD-skip ticks.
    p[PadSideStick].type = PadType::Click; p[PadSideStick].preset = ClickCdSkip;
    setMacros(p[PadSideStick], 0.75f, 0.2f, 0.7f, 0.7f, 1.0f, 0.15f, 0.1f, 0.8f);
    p[PadSideStick].level = 0.8f; p[PadSideStick].lowpassHz = 12000.0f; p[PadSideStick].reverbSend = 0.2f; p[PadSideStick].morph = 0.35f;

    // 3 Snare: ring-modulated metallic snap with a noise burst.
    p[PadSnare].type = PadType::Ping; p[PadSnare].preset = PingRing;
    setMacros(p[PadSnare], 0.5f, 0.35f, 0.58f, 0.2f, 0.62f, 0.7f, 0.4f, 0.55f);
    p[PadSnare].level = 0.8f; p[PadSnare].lowpassHz = 7000.0f; p[PadSnare].tailCutMs = 110.0f; p[PadSnare].reverbSend = 0.3f; p[PadSnare].morph = 0.25f;

    // 4 Paper: crunchy 8-bit paper.
    p[PadPaper].type = PadType::Noise; p[PadPaper].preset = NoisePaper;
    setMacros(p[PadPaper], 0.35f, 0.3f, 0.6f, 0.0f, 0.1f, 0.9f, 0.15f, 0.7f);
    p[PadPaper].level = 0.9f; p[PadPaper].lowpassHz = 8000.0f; p[PadPaper].reverbSend = 0.2f; p[PadPaper].morph = 0.3f;

    // 5 Closed hat: a tiny upward blip.
    p[PadClosedHat].type = PadType::Ping; p[PadClosedHat].preset = PingBright;
    setMacros(p[PadClosedHat], 0.9f, 0.08f, 0.38f, 0.12f, 0.4f, 0.3f, 0.2f, 0.25f);
    p[PadClosedHat].chokeGroup = 1; p[PadClosedHat].level = 0.6f; p[PadClosedHat].lowpassHz = 14000.0f;
    p[PadClosedHat].flags = PadFreezeSource; p[PadClosedHat].scatterMul = 1.5f; p[PadClosedHat].morph = 0.3f;

    // 6 Pedal hat: an 8-bit burst.
    p[PadPedalHat].type = PadType::Click; p[PadPedalHat].preset = ClickBurst;
    setMacros(p[PadPedalHat], 0.0f, 0.3f, 0.5f, 0.3f, 0.0f, 0.5f, 0.35f, 0.8f);
    p[PadPedalHat].chokeGroup = 1; p[PadPedalHat].level = 0.55f; p[PadPedalHat].lowpassHz = 11000.0f; p[PadPedalHat].morph = 0.2f;

    // 7 Open hat: resonant metallic shimmer.
    p[PadOpenHat].type = PadType::Noise; p[PadOpenHat].preset = NoiseHat;
    setMacros(p[PadOpenHat], 0.55f, 0.72f, 0.3f, 0.9f, 0.2f, 0.0f, 0.6f, 0.8f);
    p[PadOpenHat].chokeGroup = 1; p[PadOpenHat].maxPoly = 1; p[PadOpenHat].level = 0.5f; p[PadOpenHat].lowpassHz = 12000.0f; p[PadOpenHat].scatterMul = 1.5f;

    // 8 Ride: a detuned, damped bell ping.
    p[PadRide].type = PadType::Modal; p[PadRide].preset = ModalBell;
    setMacros(p[PadRide], 0.62f, 0.22f, 0.55f, 0.5f, 0.7f, 0.5f, 0.3f, 0.1f);
    p[PadRide].level = 0.55f; p[PadRide].lowpassHz = 12000.0f; p[PadRide].flags = PadFreezeSource; p[PadRide].reverbSend = 0.3f; p[PadRide].morph = 0.3f;

    // 9 Woodblock: FM percussion, inharmonic ratio.
    p[PadWoodblock].type = PadType::Ping; p[PadWoodblock].preset = PingSine;
    setMacros(p[PadWoodblock], 0.55f, 0.3f, 0.5f, 0.2f, 0.62f, 0.8f, 0.3f, 0.05f);
    p[PadWoodblock].level = 0.7f; p[PadWoodblock].lowpassHz = 10000.0f; p[PadWoodblock].reverbSend = 0.25f; p[PadWoodblock].morph = 0.35f;

    // 10 Shaker: gritty 6-bit shaker.
    p[PadShaker].type = PadType::Noise; p[PadShaker].preset = NoiseShaker;
    setMacros(p[PadShaker], 0.5f, 0.4f, 0.5f, 0.1f, 0.5f, 1.0f, 0.0f, 0.7f);
    p[PadShaker].level = 0.7f; p[PadShaker].lowpassHz = 10000.0f; p[PadShaker].scatterMul = 2.0f; p[PadShaker].morph = 0.3f;

    // 11 Glock: short, spread bowls that track the note.
    p[PadGlock].type = PadType::Modal; p[PadGlock].preset = ModalBowl;
    setMacros(p[PadGlock], 0.5f, 0.2f, 0.5f, 0.3f, 0.5f, 0.3f, 0.5f, 0.1f);
    p[PadGlock].maxPoly = 4; p[PadGlock].level = 0.6f; p[PadGlock].lowpassHz = 10000.0f;
    p[PadGlock].flags = PadFollowsNote; p[PadGlock].baseNote = 72; p[PadGlock].reverbSend = 0.4f; p[PadGlock].morph = 0.2f;

    buildDefaultNoteMap(kit);
}

void makeDefaultKit(KitParams& kit) {
    for (int i = 0; i < kNumPads; ++i) kit.pads[i] = PadParams{};
    kit.seed = 1;

    PadParams* p = kit.pads;

    // 0 Kick: 808-ish, lowpassed hard, a little drive and grain, no sub.
    p[PadKick].type = PadType::Kick; p[PadKick].preset = KickSnap;
    setMacros(p[PadKick], 0.35f, 0.45f, 0.75f, 0.35f, 0.5f, 0.35f, 0.5f, 0.4f);
    p[PadKick].maxPoly = 1; p[PadKick].level = 0.9f; p[PadKick].lowpassHz = 3000.0f;
    p[PadKick].flags = PadDuckSource; p[PadKick].scatterMul = 0.5f;

    // 1 Thumb: pitched-down thump through a blanket; toms land here and track pitch.
    p[PadThumb].type = PadType::Kick; p[PadThumb].preset = KickBlanket;
    setMacros(p[PadThumb], 0.22f, 0.3f, 0.2f, 0.15f, 0.7f, 0.25f, 0.6f, 0.5f);
    p[PadThumb].maxPoly = 2; p[PadThumb].level = 0.8f; p[PadThumb].lowpassHz = 800.0f;
    p[PadThumb].flags = PadFollowsNote; p[PadThumb].baseNote = 45;

    // 2 Side stick
    p[PadSideStick].type = PadType::Modal; p[PadSideStick].preset = ModalCrossStick;
    setMacros(p[PadSideStick], 0.5f, 0.5f, 0.45f, 0.1f, 0.6f, 0.4f, 0.4f, 0.3f);
    p[PadSideStick].level = 0.7f; p[PadSideStick].lowpassHz = 6000.0f; p[PadSideStick].reverbSend = 0.15f;

    // 3 Snare substitute: rimshot with the tail cut off.
    p[PadSnare].type = PadType::Modal; p[PadSnare].preset = ModalRimshot;
    setMacros(p[PadSnare], 0.5f, 0.55f, 0.5f, 0.2f, 0.65f, 0.3f, 0.5f, 0.5f);
    p[PadSnare].lowpassHz = 5000.0f; p[PadSnare].tailCutMs = 90.0f; p[PadSnare].reverbSend = 0.3f;
    p[PadSnare].driveDb = 4.0f;

    // 4 Paper clap
    p[PadPaper].type = PadType::Noise; p[PadPaper].preset = NoisePaper;
    setMacros(p[PadPaper], 0.3f, 0.32f, 0.5f, 0.0f, 0.2f, 0.5f, 0.2f, 0.6f);
    p[PadPaper].level = 0.9f; p[PadPaper].lowpassHz = 7000.0f; p[PadPaper].reverbSend = 0.25f;

    // 5 Closed hat: CD-skip clicks.
    p[PadClosedHat].type = PadType::Click; p[PadClosedHat].preset = ClickCdSkip;
    setMacros(p[PadClosedHat], 0.6f, 0.25f, 0.5f, 0.6f, 0.3f, 0.3f, 0.2f, 0.7f);
    p[PadClosedHat].chokeGroup = 1; p[PadClosedHat].level = 0.85f; p[PadClosedHat].lowpassHz = 9000.0f;
    p[PadClosedHat].flags = PadFreezeSource; p[PadClosedHat].scatterMul = 1.5f;

    // 6 Pedal hat: short brush.
    p[PadPedalHat].type = PadType::Noise; p[PadPedalHat].preset = NoiseBrush;
    setMacros(p[PadPedalHat], 0.55f, 0.25f, 0.2f, 0.1f, 0.3f, 0.0f, 0.6f, 0.6f);
    p[PadPedalHat].chokeGroup = 1; p[PadPedalHat].level = 0.5f; p[PadPedalHat].lowpassHz = 9000.0f;

    // 7 Open hat: longer filtered noise, one at a time.
    p[PadOpenHat].type = PadType::Noise; p[PadOpenHat].preset = NoiseHat;
    setMacros(p[PadOpenHat], 0.6f, 0.8f, 0.3f, 0.35f, 0.15f, 0.0f, 0.55f, 0.65f);
    p[PadOpenHat].chokeGroup = 1; p[PadOpenHat].maxPoly = 1; p[PadOpenHat].level = 0.5f;
    p[PadOpenHat].lowpassHz = 9000.0f; p[PadOpenHat].scatterMul = 1.5f;

    // 8 Ride: 12-bit noise burst with a 6 kHz color.
    p[PadRide].type = PadType::Click; p[PadRide].preset = ClickBurst;
    setMacros(p[PadRide], 0.78f, 0.75f, 0.3f, 1.0f, 0.0f, 0.5f, 0.2f, 0.75f);
    p[PadRide].level = 0.55f; p[PadRide].lowpassHz = 10000.0f; p[PadRide].flags = PadFreezeSource;
    p[PadRide].scatterMul = 1.5f;

    // 9 Woodblock
    p[PadWoodblock].type = PadType::Modal; p[PadWoodblock].preset = ModalWoodblock;
    setMacros(p[PadWoodblock], 0.5f, 0.5f, 0.5f, 0.15f, 0.5f, 0.3f, 0.4f, 0.2f);
    p[PadWoodblock].lowpassHz = 8000.0f; p[PadWoodblock].reverbSend = 0.2f;

    // 10 Shaker
    p[PadShaker].type = PadType::Noise; p[PadShaker].preset = NoiseShaker;
    setMacros(p[PadShaker], 0.5f, 0.4f, 0.4f, 0.0f, 0.5f, 0.6f, 0.7f, 0.6f);
    p[PadShaker].level = 0.75f; p[PadShaker].lowpassHz = 10000.0f; p[PadShaker].scatterMul = 2.0f;

    // 11 Glock: melodic ticks, tracks the note.
    p[PadGlock].type = PadType::Modal; p[PadGlock].preset = ModalGlockenspiel;
    setMacros(p[PadGlock], 0.5f, 0.55f, 0.45f, 0.05f, 0.55f, 0.2f, 0.35f, 0.15f);
    p[PadGlock].maxPoly = 4; p[PadGlock].level = 0.6f; p[PadGlock].lowpassHz = 9000.0f;
    p[PadGlock].flags = PadFollowsNote; p[PadGlock].baseNote = 72; p[PadGlock].reverbSend = 0.35f;

    buildDefaultNoteMap(kit);
}

} // namespace hic
