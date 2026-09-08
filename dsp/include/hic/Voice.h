#pragma once
#include "hic/Config.h"
#include "hic/Pad.h"
#include "hic/Math.h"
#include "hic/Filters.h"
#include "hic/Saturator.h"
#include "hic/voices/KickVoice.h"
#include "hic/voices/ClickVoice.h"
#include "hic/voices/ModalVoice.h"
#include "hic/voices/NoiseVoice.h"

namespace hic {

/// One slot in the voice pool: every generator type plus the per-voice
/// shaper (lowpass, drive, level, tail cut, choke fade, reverse, pan).
/// Generators are selected with a switch; no virtual dispatch, no heap.
class Voice {
public:
    static constexpr int kReverseSamples = nextPow2(samplesForMs(250.0f));

    void prepare(float sr) {
        sr_ = sr;
        kick_.prepare(sr); click_.prepare(sr); modal_.prepare(sr); noise_.prepare(sr);
        sat_.prepare(sr);
        active_ = false;
    }

    void trigger(const PadParams& p, int padIndex, float vel, int note, uint32_t seed, bool reverse) {
        pad_ = padIndex; note_ = note; type_ = p.type;
        reverbSend_ = p.reverbSend;
        freezeSource_ = (p.flags & PadFreezeSource) != 0;

        vel = clamp(vel, 0.0f, 1.0f);
        const float velCurve = vel * (0.5f + 0.5f * vel);
        gain_ = p.level * (1.0f - p.velToLevel + p.velToLevel * velCurve);
        const float th = (clamp(p.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
        panL_ = std::cos(th); panR_ = std::sin(th);

        lp_.set(p.lowpassHz, 0.6f, sr_); lp_.reset();
        sat_.setDrive(p.driveDb); sat_.reset();
        lpBypass_ = p.lowpassHz >= 19000.0f;

        cutAt_ = p.tailCutMs > 0.0f ? static_cast<int>(p.tailCutMs * 0.001f * sr_) : -1;
        fadeGain_ = 1.0f; fadeStep_ = 0.0f; fading_ = false;
        pos_ = 0;

        switch (type_) {
            case PadType::Kick:  kick_.trigger(p, vel, note, seed);  break;
            case PadType::Click: click_.trigger(p, vel, note, seed); break;
            case PadType::Modal: modal_.trigger(p, vel, note, seed); break;
            case PadType::Noise: noise_.trigger(p, vel, note, seed); break;
            default: break;
        }

        reverse_ = reverse || (p.flags & PadReverse);
        if (reverse_) {
            // Render the whole hit now and play it back to front.
            revLen_ = clamp(static_cast<int>(p.reverseMs * 0.001f * sr_), 16, kReverseSamples);
            int done = 0;
            while (done < revLen_) {
                const int chunk = (revLen_ - done) < kMaxBlock ? (revLen_ - done) : kMaxBlock;
                generate(revBuf_ + done, chunk);
                done += chunk;
            }
            // Fade the very end (which becomes the start) so it does not click.
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
            for (int i = 0; i < n; ++i) {
                scratch[i] = revPos_ >= 0 ? revBuf_[revPos_] : 0.0f;
                --revPos_;
            }
            if (revPos_ < 0) active_ = false;
        } else {
            generate(scratch, n);
            if (!generatorActive()) active_ = false;
        }

        for (int i = 0; i < n; ++i) {
            float x = scratch[i];
            if (!lpBypass_) x = lp_.lp(x);
            x = sat_.tick(x);
            x *= gain_;
            if (cutAt_ >= 0 && pos_ >= cutAt_ && !fading_) startFade(1.0f);
            if (fading_) {
                fadeGain_ -= fadeStep_;
                if (fadeGain_ <= 0.0f) { fadeGain_ = 0.0f; active_ = false; }
            }
            x *= fadeGain_;
            dryL[i] += x * panL_;
            dryR[i] += x * panR_;
            revSend[i] += x * reverbSend_;
            if (freezeSource_) freezeTap[i] += x;
            ++pos_;
        }
    }

    /// Fast fade to silence (choke groups, voice stealing).
    void choke(float fadeMs) { if (active_ && !fading_) startFade(fadeMs); }

    bool isActive() const { return active_; }
    bool isFading() const { return fading_; }
    int pad() const { return pad_; }
    int note() const { return note_; }
    uint32_t age() const { return age_; }
    void setAge(uint32_t a) { age_ = a; }

private:
    void generate(float* out, int n) {
        switch (type_) {
            case PadType::Kick:  kick_.render(out, n);  break;
            case PadType::Click: click_.render(out, n); break;
            case PadType::Modal: modal_.render(out, n); break;
            case PadType::Noise: noise_.render(out, n); break;
            default: for (int i = 0; i < n; ++i) out[i] = 0.0f; break;
        }
    }
    bool generatorActive() const {
        switch (type_) {
            case PadType::Kick:  return kick_.isActive();
            case PadType::Click: return click_.isActive();
            case PadType::Modal: return modal_.isActive();
            case PadType::Noise: return noise_.isActive();
            default: return false;
        }
    }
    void startFade(float ms) {
        fading_ = true;
        const float samples = ms * 0.001f * sr_;
        fadeStep_ = samples < 1.0f ? 1.0f : 1.0f / samples;
    }

    float sr_ = 48000.0f;
    PadType type_ = PadType::Click;
    int pad_ = 0, note_ = 0, pos_ = 0, cutAt_ = -1;
    uint32_t age_ = 0;
    bool active_ = false, fading_ = false, reverse_ = false, lpBypass_ = false, freezeSource_ = false;
    float gain_ = 1.0f, panL_ = 0.707f, panR_ = 0.707f, reverbSend_ = 0.0f;
    float fadeGain_ = 1.0f, fadeStep_ = 0.0f;
    int revLen_ = 0, revPos_ = 0;
    float revBuf_[kReverseSamples] = {};

    KickVoice kick_;
    ClickVoice click_;
    ModalVoice modal_;
    NoiseVoice noise_;
    TptSvf lp_;
    Saturator sat_;
};

} // namespace hic
