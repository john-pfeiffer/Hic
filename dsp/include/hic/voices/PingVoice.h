#pragma once
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"

namespace hic {

/// Synthetic micro-percussion: a sine with a fast pitch bend, optional
/// phase modulation from a second oscillator (or its own output), ring
/// modulation, and a tuned noise burst. Blips, zaps, drops and metallic
/// pings in the spirit of modular and FM percussion packs.
class PingVoice {
public:
    void prepare(float sr) { sr_ = sr; }

    void trigger(const PadParams& p, float vel, int note, uint32_t seed) {
        rng_.seed(seed);
        preset_ = p.preset;
        float tune = mapExp(p.macro[0], 40.0f, 4000.0f);
        if (p.flags & PadFollowsNote) tune *= std::exp2(static_cast<float>(note - p.baseNote) / 12.0f);
        const float decayMs  = mapExp(p.macro[1], 5.0f, 800.0f);
        const float bendSemi = (p.macro[2] - 0.5f) * 96.0f;                 // -48..48
        const float bendMs   = mapExp(p.macro[3], 3.0f, 200.0f);
        const float ratio    = mapExp(p.macro[4], 0.5f, 8.0f);
        const float fmAmt    = p.macro[5];
        const float fmMs     = mapExp(p.macro[6], 2.0f, 300.0f);
        noiseMix_            = p.macro[7];

        incC_ = tune / sr_;
        incM_ = tune * ratio / sr_;
        bendRatio_ = std::exp2(bendSemi / 12.0f) - 1.0f;
        index_ = fmAmt * (preset_ == PingFeedback ? 0.9f : 0.6f) * (0.6f + 0.4f * vel * p.velToTone + 0.4f * (1.0f - p.velToTone));
        phaseC_ = phaseM_ = 0.0f; prev_ = 0.0f;
        amp_.setDecayMs(decayMs, sr_); amp_.trigger(0.6f + 0.4f * vel);
        bend_.setDecayMs(bendMs, sr_); bend_.trigger(1.0f);
        fm_.setDecayMs(fmMs, sr_); fm_.trigger(1.0f);
        noiseEnv_.setDecayMs(decayMs * 0.35f + 1.0f, sr_); noiseEnv_.trigger(noiseMix_ > 0.001f ? 1.0f : 0.0f);
        noiseBp_.set(clamp(tune * 2.0f, 200.0f, sr_ * 0.4f), 4.0f, sr_); noiseBp_.reset();
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        for (int i = 0; i < n; ++i) {
            const float b = bend_.tick();
            const float mult = 1.0f + bendRatio_ * b;
            phaseC_ += incC_ * mult; if (phaseC_ >= 1.0f) phaseC_ -= 1.0f; else if (phaseC_ < 0.0f) phaseC_ += 1.0f;
            phaseM_ += incM_ * mult; if (phaseM_ >= 1.0f) phaseM_ -= 1.0f;
            const float fmEnv = fm_.tick() * index_;
            float y;
            switch (preset_) {
                case PingRing: {
                    const float c = fastSin01(phaseC_);
                    const float m = fastSin01(phaseM_);
                    y = c * (1.0f - fmEnv) + c * m * fmEnv * 1.5f;
                    break;
                }
                case PingFeedback: {
                    float ph = phaseC_ + prev_ * fmEnv * 0.5f;
                    ph -= std::floor(ph);
                    y = fastSin01(ph);
                    break;
                }
                case PingBright: {
                    float ph = phaseC_ + fastSin01(phaseM_) * fmEnv * 0.5f;
                    ph -= std::floor(ph);
                    const float s = fastSin01(ph);
                    y = s + 0.3f * s * s * s;   // a little extra edge
                    break;
                }
                default: {
                    float ph = phaseC_ + fastSin01(phaseM_) * fmEnv * 0.5f;
                    ph -= std::floor(ph);
                    y = fastSin01(ph);
                    break;
                }
            }
            prev_ = y;
            y *= amp_.tick();
            const float ne = noiseEnv_.tick();
            if (ne > 1e-4f) y += noiseBp_.bp(rng_.bipolar()) * ne * noiseMix_ * 2.0f;
            out[i] = y;
        }
        if (!amp_.active() && !noiseEnv_.active()) active_ = false;
    }

    bool isActive() const { return active_; }

private:
    float sr_ = 48000.0f;
    uint8_t preset_ = PingSine;
    bool active_ = false;
    float incC_ = 0.0f, incM_ = 0.0f, phaseC_ = 0.0f, phaseM_ = 0.0f, prev_ = 0.0f;
    float bendRatio_ = 0.0f, index_ = 0.0f, noiseMix_ = 0.0f;
    ExpDecay amp_, bend_, fm_, noiseEnv_;
    TptSvf noiseBp_;
    Rng rng_;
};

} // namespace hic
