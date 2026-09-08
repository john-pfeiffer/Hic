#include "hic/voices/ModalPresets.h"
#include "hic/Pad.h"

namespace hic {

// Partial ratios and relative decays. These are starting points chosen from
// the usual free-bar / struck-object literature and then nudged by ear.
static const ModalPreset kPresets[ModalPresetCount] = {
    { "Cross Stick", 1200.0f,   25.0f, 6000.0f, 0.6f, false, 4,
      { {1.0f, 1.0f, 1.0f}, {1.8f, 0.6f, 0.7f}, {2.9f, 0.4f, 0.45f}, {4.2f, 0.3f, 0.3f}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} } },
    { "Rimshot",      400.0f,   70.0f, 5000.0f, 0.8f, false, 6,
      { {1.0f, 1.0f, 1.0f}, {1.6f, 0.8f, 0.8f}, {2.2f, 0.7f, 0.6f}, {3.1f, 0.5f, 0.5f}, {4.4f, 0.4f, 0.35f}, {6.8f, 0.25f, 0.25f}, {0,0,0}, {0,0,0} } },
    { "Woodblock",    800.0f,   60.0f, 4000.0f, 0.8f, false, 4,
      { {1.0f, 1.0f, 1.0f}, {2.76f, 0.7f, 0.6f}, {5.40f, 0.4f, 0.35f}, {8.93f, 0.25f, 0.2f}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} } },
    { "Pencil Tap",  2500.0f,    8.0f, 8000.0f, 0.4f, false, 2,
      { {1.0f, 1.0f, 1.0f}, {2.3f, 0.5f, 0.5f}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} } },
    { "Muted Guitar", 196.0f,   45.0f, 2000.0f, 1.5f, true, 6,
      { {1.0f, 1.0f, 1.0f}, {2.0f, 0.5f, 0.6f}, {3.0f, 0.33f, 0.4f}, {4.0f, 0.25f, 0.3f}, {5.0f, 0.2f, 0.2f}, {6.0f, 0.17f, 0.15f}, {0,0,0}, {0,0,0} } },
    { "Glockenspiel", 1046.5f, 1200.0f, 9000.0f, 0.5f, true, 4,
      { {1.0f, 1.0f, 1.0f}, {2.71f, 0.6f, 0.5f}, {5.15f, 0.35f, 0.25f}, {8.4f, 0.2f, 0.12f}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} } },
    { "Toy Piano",    523.25f, 350.0f, 5000.0f, 1.0f, true, 6,
      { {1.0f, 1.0f, 1.0f}, {1.012f, 0.9f, 0.7f}, {2.9f, 0.5f, 0.5f}, {2.93f, 0.45f, 0.3f}, {5.6f, 0.3f, 0.25f}, {9.1f, 0.15f, 0.1f}, {0,0,0}, {0,0,0} } },
    { "Bell",         440.0f, 3000.0f, 6000.0f, 0.8f, true, 6,
      { {1.0f, 1.0f, 0.8f}, {2.0f, 0.9f, 0.7f}, {2.4f, 0.8f, 0.6f}, {3.0f, 0.7f, 0.5f}, {4.5f, 0.5f, 0.35f}, {5.3f, 0.4f, 0.25f}, {0,0,0}, {0,0,0} } },
    { "Bowl",         220.0f, 7000.0f, 3000.0f, 3.0f, true, 4,
      { {1.0f, 1.0f, 1.0f}, {2.7f, 0.8f, 0.5f}, {4.9f, 0.6f, 0.3f}, {7.8f, 0.4f, 0.15f}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} } },
};

const ModalPreset& modalPreset(int id) {
    if (id < 0 || id >= ModalPresetCount) id = 0;
    return kPresets[id];
}

int padPresetCount(PadType t) {
    switch (t) {
        case PadType::Kick:  return KickPresetCount;
        case PadType::Click: return ClickPresetCount;
        case PadType::Modal: return ModalPresetCount;
        case PadType::Noise: return NoisePresetCount;
        default: return 1;
    }
}

const char* padPresetName(PadType t, int preset) {
    static const char* const kick[KickPresetCount]   = { "Snap", "Blanket", "Plain" };
    static const char* const click[ClickPresetCount] = { "Impulse", "Dipole", "Burst", "CD Skip" };
    static const char* const noise[NoisePresetCount] = { "Hat", "Brush", "Shaker", "Paper" };
    if (preset < 0 || preset >= padPresetCount(t)) return "";
    switch (t) {
        case PadType::Kick:  return kick[preset];
        case PadType::Click: return click[preset];
        case PadType::Modal: return modalPreset(preset).name;
        case PadType::Noise: return noise[preset];
        default: return "";
    }
}

} // namespace hic
