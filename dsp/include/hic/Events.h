#pragma once
#include <cstdint>
#include "hic/Config.h"

namespace hic {

enum class EventSource : uint8_t { Midi = 0, Seq, Internal };

enum EventFlags : uint8_t {
    EvReverse   = 1 << 0,   // play this hit backwards
    EvAccent    = 1 << 1,
    EvBedGate   = 1 << 2,   // toggles the bed gate rather than playing a pad
    EvNoPad     = 1 << 3,   // control event, no voice
};

/// One hit. sampleTime is relative to the start of the current block when
/// handed to Engine::process, and absolute once inside the EventQueue.
struct NoteEvent {
    int64_t sampleTime = 0;
    uint8_t pad = 0;
    uint8_t note = 36;
    uint8_t vel = 100;
    uint8_t source = 0;
    uint8_t flags = 0;
    uint16_t step = 0;      // sequencer step index (for seeded randomness)
    uint32_t bar = 0;       // bar index (for seeded randomness)
};

/// Host transport snapshot for one block.
struct TransportInfo {
    bool   valid   = false;
    bool   playing = false;
    double ppq     = 0.0;     // beats since start at the first sample
    double bpm     = 120.0;
    int    num     = 4;
    int    den     = 4;
};

/// Fixed-capacity queue kept sorted by sampleTime. Insertion is a linear
/// shift over at most kMaxEvents entries; no allocation.
class EventQueue {
public:
    void clear() { n_ = 0; }
    int size() const { return n_; }

    bool push(const NoteEvent& e) {
        if (n_ >= kMaxEvents) return false;
        int i = n_;
        while (i > 0 && ev_[i - 1].sampleTime > e.sampleTime) { ev_[i] = ev_[i - 1]; --i; }
        ev_[i] = e;
        ++n_;
        return true;
    }

    /// Moves every event with sampleTime < before into out (at most max).
    int popDue(int64_t before, NoteEvent* out, int max) {
        int k = 0;
        while (k < n_ && k < max && ev_[k].sampleTime < before) { out[k] = ev_[k]; ++k; }
        if (k > 0) { for (int i = k; i < n_; ++i) ev_[i - k] = ev_[i]; n_ -= k; }
        return k;
    }

    const NoteEvent& peek(int i) const { return ev_[i]; }

private:
    NoteEvent ev_[kMaxEvents];
    int n_ = 0;
};

} // namespace hic
