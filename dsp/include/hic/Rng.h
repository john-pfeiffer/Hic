#pragma once
#include <cstdint>

namespace hic {

/// Mix three 32-bit values into one well-distributed seed (splitmix-style).
/// Used so that (seed, bar, step) always yields the same random stream.
inline uint32_t hashSeed(uint32_t seed, uint32_t a, uint32_t b = 0) {
    uint32_t z = seed ^ (a * 0x9E3779B9u) ^ (b * 0x85EBCA6Bu) ^ 0x6A09E667u;
    z ^= z >> 16; z *= 0x7FEB352Du;
    z ^= z >> 15; z *= 0x846CA68Bu;
    z ^= z >> 16;
    return z == 0u ? 0x1234567u : z;
}

/// xorshift32. Tiny, fast, good enough for audio noise and musical dice.
class Rng {
public:
    Rng() = default;
    explicit Rng(uint32_t s) { seed(s); }

    void seed(uint32_t s) { state_ = (s == 0u) ? 0x1234567u : s; }

    uint32_t next() {
        uint32_t x = state_;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return state_ = x;
    }
    /// [0, 1)
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    /// [-1, 1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }
    /// Roughly gaussian in [-1, 1], cheap (sum of three uniforms).
    float gauss3() { return (uniform() + uniform() + uniform()) * (2.0f / 3.0f) - 1.0f; }
    /// true with probability p
    bool chance(float p) { return uniform() < p; }

    uint32_t state() const { return state_; }

private:
    uint32_t state_ = 0x1234567u;
};

} // namespace hic
