#pragma once
// 16-bit PCM WAV writer, mono or stereo. Test/tool use only.
#include <cstdio>
#include <cstdint>
#include <cmath>

namespace hictest {

inline void put16(std::FILE* f, uint16_t v) { uint8_t b[2] = { uint8_t(v & 0xff), uint8_t(v >> 8) }; std::fwrite(b, 1, 2, f); }
inline void put32(std::FILE* f, uint32_t v) { uint8_t b[4] = { uint8_t(v & 0xff), uint8_t((v >> 8) & 0xff), uint8_t((v >> 16) & 0xff), uint8_t(v >> 24) }; std::fwrite(b, 1, 4, f); }

/// Writes `n` frames. Pass right = nullptr for mono.
inline bool writeWav(const char* path, const float* left, const float* right, int n, int sampleRate) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const uint16_t channels = right ? 2 : 1;
    const uint32_t dataBytes = uint32_t(n) * channels * 2u;
    std::fwrite("RIFF", 1, 4, f); put32(f, 36u + dataBytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); put32(f, 16u); put16(f, 1u); put16(f, channels);
    put32(f, uint32_t(sampleRate)); put32(f, uint32_t(sampleRate) * channels * 2u); put16(f, uint16_t(channels * 2)); put16(f, 16u);
    std::fwrite("data", 1, 4, f); put32(f, dataBytes);
    for (int i = 0; i < n; ++i) {
        for (int c = 0; c < channels; ++c) {
            float v = c == 0 ? left[i] : right[i];
            if (!(v == v)) v = 0.0f;
            v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
            put16(f, uint16_t(int16_t(std::lround(v * 32767.0f))));
        }
    }
    std::fclose(f);
    return true;
}

} // namespace hictest
