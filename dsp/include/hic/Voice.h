#pragma once
#include "hic/Config.h"
#include "hic/Pad.h"
#include "hic/Key.h"
#include "hic/Math.h"
#include "hic/UnifiedVoice.h"

namespace hic {

/// One slot in the voice pool: the unified voice plus the thin wrapper
/// (level and velocity, choke fade, reverse playback, pan and sends).
class Voice {
public:
    static constexpr int kReverseSamples = nextPow2(samplesForMs(250.0f));

    void prepare(float sr) { sr_ = sr; gen_.prepare(sr); active_ = false; }

    void trigger(const PadParams& p, int padIndex, float vel, int note, uint32_t seed, bool reverse,
                 const KeyParams& key, float driftMul, float texture) {
        pad_ = padIndex; note_ = note;
        reverbSend_ = p.reverbSend;
        freezeSource_ = (p.flags & PadFreezeSource) != 0;

        vel = clamp(vel, 0.0f, 1.0f);
        const float velCurve = vel * (0.5f + 0.5f * vel);
        gain_ = p.level * (1.0f - p.velToLevel + p.velToLevel * velCurve);
        const float th = (clamp(p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
        panL_ = std::cos(th); panR_ = std::sin(th);
        fadeGain_ = 1.0f; fadeStep_ = 0.0f; fading_ = false;

        gen_.trigger(p, vel, note, seed, key, driftMul, texture);

        reverse_ = reverse || (p.flags & PadReverse);
        if (reverse_) {
            revLen_ = clamp(static_cast<int>(p.reverseMs * 0.001f * sr_), 16, kReverseSamples);
            int done = 0;
            while (done < revLen_) {
                const int chunk = (revLen_ - done) < kMaxBlock ? (revLen_ - done) : kMaxBlock;
                gen_.render(revBuf_ + done, chunk);
                done += chunk;
            }
            const int fade = revLen_ < 64 ? revLen_ : 64;
            for (int i = 0; i < fade; ++i) revBuf_[revLen_ - 1 - i] *= static_cast<float>(i) / static_cast<float>(fade);
            revPos_ = revLen_ - 1;
        }
        active_ = true;
        ++age_;
    }

    /// Accumulates into the stereo dry buffers, the reverb send and the freeze tap.
    void render(float* dryL, float* dryR, float* revSend, float* freezeTap, float* scratch, int n) {
        if (!active_) return;
        if (reverse_) {
            for (int i = 0; i < n; ++i) { scratch[i] = revPos_ >= 0 ? revBuf_[revPos_] : 0.0f; --revPos_; }
            if (revPos_ < 0) active_ = false;
        } else {
            gen_.render(scratch, n);
            if (!gen_.isActive()) active_ = false;
        }
        for (int i = 0; i < n; ++i) {
            float x = scratch[i] * gain_;
            if (fading_) {
                fadeGain_ -= fadeStep_;
                if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; active_ = false; }
            }
            x *= fadeGain_;
            dryL[i] += x * panL_;
            dryR[i] += x * panR_;
            revSend[i] += x * reverbSend_;
            if (freezeSource_) freezeTap[i] += x;
        }
    }

    void choke(float fadeMs) {
        if (!active_ || fading_) return;
        fading_ = true;
        const float samples = fadeMs * 0.001f * sr_;
        fadeStep_ = samples < 1.0f ? 1.0f : 1.0f / samples;
    }

    bool isActive() const { return active_; }
    bool isFading() const { return fading_; }
    int pad() const { return pad_; }
    int note() const { return note_; }
    uint32_t age() const { return age_; }
    void setAge(uint32_t a) { age_ = a; }
    const UnifiedVoice& generator() const { return gen_; }

private:
    float sr_ = 48000.0f;
    int pad_ = 0, note_ = 0;
    uint32_t age_ = 0;
    bool active_ = false, fading_ = false, reverse_ = false, freezeSource_ = false;
    float gain_ = 1.0f, panL_ = 0.707f, panR_ = 0.707f, reverbSend_ = 0.0f;
    float fadeGain_ = 1.0f, fadeStep_ = 0.0f;
    int revLen_ = 0, revPos_ = 0;
    float revBuf_[kReverseSamples] = {};
    UnifiedVoice gen_;
};

} // namespace hic
