#pragma once
#include "hic/Config.h"
#include "hic/Events.h"
#include "hic/Kit.h"
#include "hic/seq/Pattern.h"
#include "hic/seq/Clock.h"

namespace hic {

/// Turns a pattern into note events, sample-accurately, for one block.
/// Each step has a nominal time (with swing) plus its own nudge; an event is
/// emitted in the block its fire time falls in, and a per-track "last
/// emitted" guard makes the result independent of the block size.
class Sequencer {
public:
    void prepare(float sr) { sr_ = sr; reset(); }
    void reset() { for (auto& l : lastEmitted_) l = kNone; }

    /// Emits into `out` (sampleTime relative to the block start). Returns the count.
    int process(const Pattern& p, const KitParams& kit, const Clock& clock, int n, uint32_t seed, NoteEvent* out, int max);

    /// True when any track's current step carries the BedGate flag.
    static bool bedGateAt(const Pattern& p, double ppq);

    /// Step index (0..length-1) a track is on at `ppq`, for playheads.
    static int stepAt(const Pattern& p, int track, double ppq);

    /// Nominal fire time in beats of an absolute step, including swing (not nudge).
    static double fireBeats(const Pattern& p, int64_t absStep);

private:
    static constexpr int64_t kNone = INT64_MIN / 2;
    float sr_ = 48000.0f;
    int64_t lastEmitted_[kNumPads] = {};
};

} // namespace hic
