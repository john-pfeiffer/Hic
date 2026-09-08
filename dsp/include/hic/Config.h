#pragma once
// Compile-time sizing for every fixed buffer in the core. A hardware build
// sets HIC_MAX_SAMPLE_RATE=48000 to shrink the footprint.

#ifndef HIC_MAX_SAMPLE_RATE
#define HIC_MAX_SAMPLE_RATE 96000
#endif

namespace hic {

constexpr int   kMaxSampleRate = HIC_MAX_SAMPLE_RATE;
constexpr int   kMaxBlock      = 4096;   // largest block process() accepts
constexpr int   kNumVoices     = 16;     // fixed voice pool
constexpr int   kNumPads       = 12;     // pads in a kit
constexpr int   kMaxEvents     = 128;    // pending note events
constexpr float kMinSampleRate = 22050.0f;

/// Samples needed to hold `ms` milliseconds at the maximum sample rate.
constexpr int samplesForMs(float ms) { return static_cast<int>(kMaxSampleRate * ms / 1000.0f) + 1; }

/// Smallest power of two >= n (n > 0).
constexpr int nextPow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

const char* versionString();

} // namespace hic
