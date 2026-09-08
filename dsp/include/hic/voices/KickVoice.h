#pragma once
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/Redux.h"
#include "hic/Saturator.h"

namespace hic {

/// Soft, filtered kick. A sine with a fast pitch envelope, an optional
/// attack transient (snap click or blanket thump), a hard lowpass, tape
/// saturation, a 12-bit resampler and a 35 Hz highpass so there is never
/// any sub emphasis.
class KickVoice {
public:
    void prepare(float sr) {
        sr_ = sr;
        sat_.prepare(sr); redux_.prepare(sr);
        hp_.setLp(35.0f, sr);
        blanketLp_.setLp(200.0f, sr);
    }

    void trigger(const PadParams& p, float vel, int note, uint32_t seed) {
        rng_.seed(seed);
        float tune = mapExp(p.macro[0], 35.0f, 110.0f);
        if (p.flags & PadFollowsNote)
            tune *= std::exp2(static_cast<float>(note - p.baseNote) * (0.5f / 12.0f)); // half-scale tracking for toms
        const float decayMs    = mapExp(p.macro[1], 80.0f, 600.0f);
        float toneHz           = mapExp(p.macro[2], 300.0f, 4000.0f);
        const float pitchSemis = mapLin(p.macro[3], 0.0f, 36.0f);
        attackAmt_             = p.macro[4];
        const float driveDb    = mapLin(p.macro[5], 0.0f, 18.0f);
        const float pitchMs    = mapExp(p.macro[7], 8.0f, 80.0f);

        toneHz *= 1.0f + p.velToTone * (vel - 0.7f);  // harder hits open the filter slightly
        tone_.set(toneHz, 0.6f, sr_); tone_.reset();
        sat_.setDrive(driveDb); sat_.reset();
        redux_.setAmount(p.macro[6]); redux_.reset();
        hp_.reset(); blanketLp_.reset();

        phase_ = 0.0f;
        baseInc_ = tune / sr_;
        pitchRatio_ = std::exp2(pitchSemis / 12.0f) - 1.0f;
        amp_.setDecayMs(decayMs, sr_); amp_.trigger(1.0f);
        pitch_.setDecayMs(pitchMs, sr_); pitch_.trigger(1.0f);
        preset_ = p.preset;
        attack_.setDecayMs(preset_ == KickBlanket ? 8.0f : 2.0f, sr_);
        attack_.trigger(preset_ == KickPlain ? 0.0f : 1.0f);
        snapHp_.setLp(1500.0f, sr_); snapHp_.reset();
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        for (int i = 0; i < n; ++i) {
            const float pe = pitch_.tick();
            phase_ += baseInc_ * (1.0f + pitchRatio_ * pe);
            if (phase_ >= 1.0f) phase_ -= 1.0f;
            float x = fastSin01(phase_) * amp_.tick();
            const float ae = attack_.tick();
            if (ae > 1e-4f) {
                const float nz = rng_.bipolar();
                if (preset_ == KickBlanket) x += blanketLp_.lp(nz) * ae * attackAmt_ * 3.0f;
                else                        x += snapHp_.hp(nz) * ae * attackAmt_ * 0.6f;
            }
            x = tone_.lp(x);
            x = sat_.tick(x);
            x = redux_.tick(x);
            out[i] = hp_.hp(x);
        }
        if (!amp_.active() && !attack_.active()) active_ = false;
    }

    bool isActive() const { return active_; }

private:
    float sr_ = 48000.0f;
    float phase_ = 0.0f, baseInc_ = 0.0f, pitchRatio_ = 0.0f, attackAmt_ = 0.5f;
    uint8_t preset_ = KickSnap;
    bool active_ = false;
    ExpDecay amp_, pitch_, attack_;
    TptSvf tone_;
    OnePole hp_, blanketLp_, snapHp_;
    Saturator sat_;
    Redux redux_;
    Rng rng_;
};

} // namespace hic
