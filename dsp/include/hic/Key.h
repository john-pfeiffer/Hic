#pragma once
#include <cmath>
#include <cstdint>
#include "hic/Math.h"

namespace hic {

enum KeyScale : uint8_t { ScaleChromatic = 0, ScaleMinorPent, ScaleMajorPent, ScaleDorian, ScaleMinor, ScaleMajor, ScaleCount };

/// Global key: pads with the FollowKey flag snap their Tune to this.
struct KeyParams {
    uint8_t root  = 0;   // 0 = C .. 11 = B
    uint8_t scale = ScaleMinorPent;
};

inline const char* keyScaleName(int s) {
    static const char* const names[ScaleCount] = { "Chromatic", "Minor Pentatonic", "Major Pentatonic", "Dorian", "Minor", "Major" };
    return (s >= 0 && s < ScaleCount) ? names[s] : "";
}
inline const char* noteName(int n) {
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((n % 12) + 12) % 12];
}

/// Is `degree` (0..11 above the root) in the scale?
inline bool inScale(int scale, int degree) {
    static const uint8_t lists[ScaleCount][12] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
        { 0, 3, 5, 7, 10, 255, 255, 255, 255, 255, 255, 255 },
        { 0, 2, 4, 7, 9, 255, 255, 255, 255, 255, 255, 255 },
        { 0, 2, 3, 5, 7, 9, 10, 255, 255, 255, 255, 255 },
        { 0, 2, 3, 5, 7, 8, 10, 255, 255, 255, 255, 255 },
        { 0, 2, 4, 5, 7, 9, 11, 255, 255, 255, 255, 255 },
    };
    if (scale < 0 || scale >= ScaleCount) return true;
    for (int i = 0; i < 12 && lists[scale][i] != 255; ++i) if (lists[scale][i] == degree) return true;
    return false;
}

/// Nearest MIDI note in the key (ties resolve downward).
inline int quantizeMidi(float midi, const KeyParams& k) {
    const int n = static_cast<int>(std::floor(midi + 0.5f));
    for (int d = 0; d <= 6; ++d) {
        const int lo = n - d, hi = n + d;
        if (inScale(k.scale, (((lo - k.root) % 12) + 12) % 12)) return lo;
        if (inScale(k.scale, (((hi - k.root) % 12) + 12) % 12)) return hi;
    }
    return n;
}

/// Snap a frequency to the key. Trigger-time only (uses log2).
inline float quantizeHz(float hz, const KeyParams& k) {
    if (hz <= 0.0f) return hz;
    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    return midiToHz(static_cast<float>(quantizeMidi(midi, k)));
}

} // namespace hic
