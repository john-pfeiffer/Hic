#pragma once
#include "hic/Config.h"
#include "hic/Pad.h"
#include "hic/Bus.h"

namespace hic {

/// A kit: twelve pads, the MIDI note map, and the bus setting it was voiced with.
struct KitParams {
    PadParams pads[kNumPads];
    uint8_t noteToPad[128] = {};
    uint8_t seed = 1;
    BusParams bus;
};

enum KitId : uint8_t { KitNeon = 0, KitMicro, KitModular, KitCount };

void makeKit(KitId id, KitParams& kit);
const char* kitName(KitId id);

/// Neon: Notwist / Lali Puna. Damped, woody, clicks. General MIDI compatible.
void makeDefaultKit(KitParams& kit);
/// Micro: metal, folded, drifting synthetic micro-percussion.
void makeMicroKit(KitParams& kit);
/// Modular: laser and boing tails, everything keyed.
void makeModularKit(KitParams& kit);

/// Fills noteToPad from explicit (note, pad) pairs, nearest-note fallback.
void buildNoteMap(KitParams& kit, const uint8_t* notes, const uint8_t* pads, int count);

enum DefaultPad : uint8_t {
    PadKick = 0, PadThumb, PadSideStick, PadSnare, PadPaper, PadClosedHat,
    PadPedalHat, PadOpenHat, PadRide, PadWoodblock, PadShaker, PadGlock
};

const char* defaultPadName(int pad);

} // namespace hic
