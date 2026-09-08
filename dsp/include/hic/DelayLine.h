#pragma once
#include <array>
#include <cmath>
#include "hic/Config.h"

namespace hic {

/// Fixed-size power-of-two ring buffer. read(d) returns the sample written d
/// ticks ago (d = 0 is the most recent write).
template <int N>
class DelayLine {
    static_assert((N & (N - 1)) == 0 && N > 0, "DelayLine size must be a power of two");
public:
    static constexpr int size = N;
    static constexpr int mask = N - 1;

    void clear() { buf_.fill(0.0f); w_ = 0; }
    void write(float x) { buf_[static_cast<size_t>(w_)] = x; w_ = (w_ + 1) & mask; }

    float read(int d) const { return buf_[static_cast<size_t>((w_ - 1 - d) & mask)]; }

    float readLin(float d) const {
        const int   i = static_cast<int>(d);
        const float f = d - static_cast<float>(i);
        return read(i) + (read(i + 1) - read(i)) * f;
    }
    /// Absolute-position access for granular playback.
    float at(int pos) const { return buf_[static_cast<size_t>(pos & mask)]; }
    int writePos() const { return w_; }

private:
    std::array<float, static_cast<size_t>(N)> buf_{};
    int w_ = 0;
};

} // namespace hic
