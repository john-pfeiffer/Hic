#pragma once
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/voices/ModalPresets.h"

namespace hic {

/// Struck-object synthesis: a short noise burst excites up to eight tuned,
/// damped resonators. One algorithm covers cross-stick, rimshot,
/// woodblock, pencil tap, glockenspiel, toy piano, bells and bowls.
class ModalVoice {
public:
    void prepare(float sr) { sr_ = sr; }

    void trigger(const PadParams& p, float vel, int note, uint32_t seed) {
        rng_.seed(seed);
        const ModalPreset& ps = modalPreset(p.preset);
        const float base     = (p.flags & PadFollowsNote) ? midiToHz(static_cast<float>(note)) : ps.baseHz;
        const float tune     = base * std::exp2((p.macro[0] - 0.5f) * 2.0f);        // +-1 octave
        const float decayMul = mapExp(p.macro[1], 0.1f, 4.0f);
        const float bright   = (p.macro[2] - 0.5f);                                  // -0.5..0.5
        const float spread   = p.macro[3] * 0.02f;
        float hardHz         = mapExp(p.macro[4], 800.0f, 10000.0f);
        const float damp     = p.macro[5];
        const float strikeMs = mapExp(p.macro[6], 0.3f, 5.0f);
        stickMix_            = p.macro[7] * 0.5f;

        hardHz *= 1.0f + p.velToTone * (vel - 0.7f);
        exciterLp_.setLp(hardHz, sr_); exciterLp_.reset();
        strike_.setDecayMs(strikeMs, sr_);
        strike_.trigger(0.6f + 0.4f * vel);

        count_ = ps.count;
        for (int i = 0; i < kModalPartials; ++i) {
            if (i >= count_) { gain_[i] = 0.0f; res_[i].set(0.0f, 0.0f, sr_); continue; }
            const ModalPartial& pt = ps.p[i];
            const float hz  = tune * pt.ratio * (1.0f + spread * rng_.bipolar());
            float t60 = ps.t60Ms * 0.001f * pt.decayRatio * decayMul;
            if (hz > 3000.0f) t60 *= (1.0f - 0.9f * damp);
            res_[i].set(hz, t60, sr_); res_[i].reset();
            gain_[i] = pt.gain * std::exp2(bright * 2.0f * std::log2(pt.ratio));
        }
        // Keep the summed partials from peaking far above the fundamental alone.
        float sum = 0.0f;
        for (int i = 0; i < count_; ++i) sum += gain_[i];
        const float norm = sum * 0.6f;
        if (norm > 1.0f) for (int i = 0; i < count_; ++i) gain_[i] /= norm;
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        for (int i = 0; i < n; ++i) {
            const float exc = strike_.active() ? exciterLp_.lp(rng_.bipolar() * strike_.tick()) : 0.0f;
            float y = exc * stickMix_;
            for (int k = 0; k < count_; ++k) y += gain_[k] * res_[k].tick(exc);
            out[i] = y;
        }
        if (!strike_.active()) {
            bool quiet = true;
            for (int k = 0; k < count_; ++k) {
                if (res_[k].quiet()) res_[k].reset();   // keep decayed partials out of denormal territory
                else quiet = false;
            }
            if (quiet) active_ = false;
        }
    }

    bool isActive() const { return active_; }

private:
    float sr_ = 48000.0f;
    float gain_[kModalPartials] = {};
    float stickMix_ = 0.0f;
    int count_ = 0;
    bool active_ = false;
    Resonator res_[kModalPartials];
    ExpDecay strike_;
    OnePole exciterLp_;
    Rng rng_;
};

} // namespace hic
