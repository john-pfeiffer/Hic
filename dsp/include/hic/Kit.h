#pragma once
#include "hic/Config.h"
#include "hic/Pad.h"

namespace hic {

/// A kit: twelve pads plus the MIDI note map.
struct KitParams {
    PadParams pads[kNumPads];
    uint8_t noteToPad[128] = {};
    uint8_t seed = 1;            // default feel seed lives with the kit
};

/// The default "Neon" kit: General MIDI compatible so any existing pattern
/// plays. Unmapped notes fall to the nearest mapped note.
void makeDefaultKit(KitParams& kit);

/// "Micro": synthetic, morphing micro-percussion on the same GM map. Blips,
/// zaps, detuned pings, crunchy noise, in the spirit of modular and FM
/// percussion sample packs.
void makeMicroKit(KitParams& kit);

enum KitId : uint8_t { KitNeon = 0, KitMicro, KitCount };
void makeKit(KitId id, KitParams& kit);
const char* kitName(KitId id);

/// Fills noteToPad from explicit (note, pad) pairs, nearest-note fallback.
void buildNoteMap(KitParams& kit, const uint8_t* notes, const uint8_t* pads, int count);

enum DefaultPad : uint8_t {
    PadKick = 0, PadThumb, PadSideStick, PadSnare, PadPaper, PadClosedHat,
    PadPedalHat, PadOpenHat, PadRide, PadWoodblock, PadShaker, PadGlock
};

const char* defaultPadName(int pad);

} // namespace hic
