#pragma once
#include <cstdint>

namespace hic {

constexpr int kModalPartials = 8;

struct ModalPartial { float ratio, decayRatio, gain; };

struct ModalPreset {
    const char*  name;
    float        baseHz;        // used when the pad does not follow the note
    float        t60Ms;         // decay of the fundamental
    float        exciterLpHz;   // default strike hardness
    float        strikeMs;      // default strike length
    bool         followsNote;   // sensible default for the pad flag
    int          count;
    ModalPartial p[kModalPartials];
};

enum ModalPresetId : uint8_t {
    ModalCrossStick = 0, ModalRimshot, ModalWoodblock, ModalPencilTap, ModalMutedGuitar,
    ModalGlockenspiel, ModalToyPiano, ModalBell, ModalBowl, ModalPresetCount
};

const ModalPreset& modalPreset(int id);

} // namespace hic
