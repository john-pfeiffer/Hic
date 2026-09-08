#pragma once
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/Redux.h"

namespace hic {

/// Clicks and pops: single-sample impulses, dipoles, reduced-bit noise
/// bursts, and CD-skip impulse trains. Optionally rung through a bandpass.
/// Deliberately has no body.
class ClickVoice {
public:
    static constexpr int kMaxImpulses = 6;

    void prepare(float sr) { sr_ = sr; redux_.prepare(sr); }

    void trigger(const PadParams& p, float vel, int /*note*/, uint32_t seed) {
        rng_.seed(seed);
        preset_ = p.preset;
        const float color   = p.macro[0];
        const float decayMs = mapExp(p.macro[1], 1.0f, 40.0f);
        const float ringQ   = mapExp(p.macro[2], 0.5f, 8.0f);
        const int   bits    = 6 + static_cast<int>(p.macro[3] * 6.0f + 0.5f);   // 6..12
        const int   count   = 2 + static_cast<int>(p.macro[4] * 4.0f + 0.5f);   // 2..6
        const float spread  = mapExp(p.macro[5], 0.5f, 4.0f);                   // ms
        const int   rateDiv = 1 + static_cast<int>(p.macro[6] * 5.0f + 0.5f);   // 1..6
        const float toneHz  = mapExp(p.macro[7], 1500.0f, 20000.0f);

        useRing_ = color > 0.02f;
        float ringHz = mapExp(clamp(color, 0.02f, 1.0f), 1000.0f, 10000.0f);
        ringHz *= 1.0f + p.velToTone * (vel - 0.7f) * 0.5f;
        ring_.set(ringHz, ringQ, sr_); ring_.reset();
        tone_.set(toneHz, 0.6f, sr_); tone_.reset();
        redux_.set(bits, rateDiv, 0.6f); redux_.reset();

        // Impulse schedule (in samples from trigger) and amplitudes.
        nImp_ = 0;
        const float velGain = 0.5f + 0.5f * vel;
        if (preset_ == ClickCdSkip) {
            int t = 0;
            for (int k = 0; k < count && k < kMaxImpulses; ++k) {
                impT_[nImp_] = t;
                impA_[nImp_] = velGain * (0.6f + 0.4f * rng_.uniform()) * (rng_.chance(0.5f) ? 1.0f : -1.0f);
                ++nImp_;
                t += static_cast<int>((0.5f + rng_.uniform()) * spread * 0.001f * sr_) + 1;
            }
        } else {
            impT_[0] = 0; impA_[0] = velGain; nImp_ = 1;
            if (preset_ == ClickDipole) { impT_[1] = 1; impA_[1] = -velGain; nImp_ = 2; }
        }
        burst_.setDecayMs(decayMs, sr_);
        burst_.trigger(preset_ == ClickBurst ? velGain : 0.0f);

        const int decaySamples = static_cast<int>(decayMs * 0.001f * sr_);
        const int ringSamples  = useRing_ ? static_cast<int>(sr_ * ringQ / ringHz * 6.0f) : 0;
        length_ = impT_[nImp_ - 1] + decaySamples + ringSamples + 64;
        pos_ = 0; next_ = 0;
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        for (int i = 0; i < n; ++i) {
            float x = 0.0f;
            while (next_ < nImp_ && impT_[next_] == pos_) { x += impA_[next_]; ++next_; }
            if (preset_ == ClickBurst) x = rng_.bipolar() * burst_.tick();
            if (useRing_) x = ring_.bp(x) * 2.0f;
            x = redux_.tick(x);
            out[i] = tone_.lp(x);
            ++pos_;
        }
        if (pos_ >= length_) active_ = false;
    }

    bool isActive() const { return active_; }

private:
    float sr_ = 48000.0f;
    uint8_t preset_ = ClickImpulse;
    bool useRing_ = false, active_ = false;
    int impT_[kMaxImpulses] = {};
    float impA_[kMaxImpulses] = {};
    int nImp_ = 0, next_ = 0, pos_ = 0, length_ = 0;
    ExpDecay burst_;
    TptSvf ring_, tone_;
    Redux redux_;
    Rng rng_;
};

} // namespace hic
