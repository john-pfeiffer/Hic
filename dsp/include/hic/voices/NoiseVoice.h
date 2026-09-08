#pragma once
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/Redux.h"

namespace hic {

/// Hats, brushes, shakers and paper: shaped noise with very fast envelopes,
/// a highpass, an optional band emphasis, a touch of metallic resonance and
/// bit reduction.
class NoiseVoice {
public:
    void prepare(float sr) { sr_ = sr; redux_.prepare(sr); }

    void trigger(const PadParams& p, float vel, int /*note*/, uint32_t seed) {
        rng_.seed(seed); rng2_.seed(hashSeed(seed, 77u));
        preset_ = p.preset;
        float hpHz          = mapExp(p.macro[0], 2000.0f, 12000.0f);
        const float decayMs = mapExp(p.macro[1], 5.0f, 300.0f);
        bandMix_            = p.macro[2];
        metalMix_           = p.macro[3] * 0.7f;
        float attackMs      = mapExp(p.macro[4], 0.1f, 10.0f);
        grit_               = p.macro[5];
        const int bits      = 6 + static_cast<int>(p.macro[6] * 10.0f + 0.5f);  // 6..16
        const float toneHz  = mapExp(p.macro[7], 3000.0f, 20000.0f);

        hpHz *= 1.0f + p.velToTone * (vel - 0.7f) * 0.5f;
        if (preset_ == NoiseBrush) attackMs = attackMs * 2.0f + 2.0f;
        hp_.set(hpHz, 0.7f, sr_); hp_.reset(); hp2_.set(hpHz, 0.7f, sr_); hp2_.reset();
        band_.set(hpHz * 0.9f, 3.0f, sr_); band_.reset();
        tone_.set(toneHz, 0.6f, sr_); tone_.reset();
        brushLp_.setLp(6000.0f, sr_); brushLp_.reset();
        gritLp_.setLp(mapLin(grit_, 60.0f, 250.0f), sr_); gritLp_.reset();
        const float metalBase = 7000.0f;
        metal_[0].set(metalBase, 0.02f, sr_);          metal_[0].reset();
        metal_[1].set(metalBase * 1.42f, 0.015f, sr_); metal_[1].reset();
        metal_[2].set(metalBase * 1.9f, 0.012f, sr_);  metal_[2].reset();
        redux_.set(bits, 1, 0.5f); redux_.reset();

        env_.setAttackMs(attackMs, sr_);
        env_.setReleaseMs(decayMs, sr_);
        env_.reset(0.0f); env_.gate(true);
        hold_ = static_cast<int>(attackMs * 0.001f * sr_) + 1;
        pos_ = 0;
        gain_ = 0.5f + 0.5f * vel;
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        for (int i = 0; i < n; ++i) {
            if (pos_ == hold_) env_.gate(false);
            float nz = rng_.bipolar();
            switch (preset_) {
                case NoiseBrush:  nz = brushLp_.lp(nz) * 2.0f; break;
                case NoiseShaker: nz *= 0.35f + grit_ * 2.0f * std::fabs(gritLp_.lp(rng2_.bipolar()) * 6.0f); break;
                case NoisePaper:  nz = rng2_.chance(0.35f + 0.3f * (1.0f - grit_)) ? nz * 1.6f : 0.0f; break;
                default: break;
            }
            float x = hp2_.hp(hp_.hp(nz));   // 24 dB/oct: hats live above the highpass
            if (bandMix_ > 0.01f) x += band_.bp(nz) * bandMix_ * 2.0f;
            if (metalMix_ > 0.01f) {
                const float m = metal_[0].tick(x) + metal_[1].tick(x) + metal_[2].tick(x);
                x += m * metalMix_ * 0.5f;
            }
            x *= env_.tick() * gain_;
            x = redux_.tick(x);
            out[i] = tone_.lp(x);
            ++pos_;
        }
        for (auto& m : metal_) if (m.quiet()) m.reset();
        if (!env_.active()) active_ = false;
    }

    bool isActive() const { return active_; }

private:
    float sr_ = 48000.0f;
    uint8_t preset_ = NoiseHat;
    bool active_ = false;
    float bandMix_ = 0.0f, metalMix_ = 0.0f, grit_ = 0.0f, gain_ = 1.0f;
    int hold_ = 0, pos_ = 0;
    TptSvf hp_, hp2_, band_, tone_;
    OnePole brushLp_, gritLp_;
    Resonator metal_[3];
    ArEnv env_;
    Redux redux_;
    Rng rng_, rng2_;
};

} // namespace hic
