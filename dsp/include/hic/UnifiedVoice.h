#pragma once
#include <cmath>
#include "hic/Config.h"
#include "hic/Pad.h"
#include "hic/Key.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/Redux.h"

namespace hic {

/// The one voice every pad uses: exciter -> resonator -> shaper, with
/// per-hit drift, a grit layer and key quantisation resolved at trigger.
///
///  Tune     fundamental, 30 Hz .. 3 kHz
///  Decay    overall length, 5 ms .. 2 s (body and modes scale together)
///  Exciter  click -> mallet thump -> noise burst
///  Body     membrane with pitch drop -> harmonic modes (wood) -> inharmonic metal -> hat
///  Break    wavefold + saturation, feedback into the resonator above 0.5
///  Drift    seeded per-hit re-roll of all of the above
class UnifiedVoice {
public:
    static constexpr int kModes = 6;
    static constexpr int kMaxImpulses = 4;
    static constexpr int kGuardInterval = 32;

    void prepare(float sr) { sr_ = sr; gritRedux_.prepare(sr); active_ = false; }

    void trigger(const PadParams& p, float vel, int note, uint32_t seed, const KeyParams& key, float driftMul, float texture) {
        vel = clamp(vel, 0.0f, 1.0f);
        rng_.seed(seed);
        rng2_.seed(hashSeed(seed, 0x67726974u));
        for (int i = 0; i < kNumMacros; ++i) m_[i] = clamp(p.macro[i], 0.0f, 1.0f);

        // Drift: deterministic per hit, before key quantisation.
        const float amt = clamp(m_[MacroDrift] * driftMul, 0.0f, 2.0f);
        if (amt > 0.001f) {
            Rng r(hashSeed(seed, 0x44524946u));
            m_[MacroTune]    = clamp(m_[MacroTune]    + amt * 0.08f * r.gauss3(), 0.0f, 1.0f);
            m_[MacroDecay]   = clamp(m_[MacroDecay]   + amt * 0.15f * r.gauss3(), 0.0f, 1.0f);
            m_[MacroExciter] = clamp(m_[MacroExciter] + amt * 0.20f * r.gauss3(), 0.0f, 1.0f);
            m_[MacroBody]    = clamp(m_[MacroBody]    + amt * 0.12f * r.gauss3(), 0.0f, 1.0f);
            m_[MacroBreak]   = clamp(m_[MacroBreak]   + amt * 0.15f * r.gauss3(), 0.0f, 1.0f);
        }

        // Tune
        float f0 = tuneToHz(m_[MacroTune]);
        if (p.flags & PadFollowsNote) f0 *= std::exp2(static_cast<float>(note - p.baseNote) / 12.0f);
        if (p.flags & PadFollowKey) f0 = quantizeHz(f0, key);
        f0_ = clamp(f0, 20.0f, sr_ * 0.4f);

        // Decay
        const float T = decayToMs(m_[MacroDecay]);

        // Exciter
        const float x = m_[MacroExciter];
        wClick_  = 1.0f - smoothstep(x, 0.25f, 0.55f);
        wMallet_ = smoothstep(x, 0.15f, 0.45f) * (1.0f - smoothstep(x, 0.5f, 0.8f));
        wNoise_  = smoothstep(x, 0.45f, 0.85f);
        excAmp_  = 0.6f + 0.4f * vel;
        noiseGain_ = kNoise;
        nImp_ = 1 + static_cast<int>(smoothstep(x, 0.05f, 0.3f) * 3.0f + 0.5f);
        if (nImp_ > kMaxImpulses) nImp_ = kMaxImpulses;
        {
            const float spread = 0.4f + 6.0f * x;   // ms
            int t = 0; float a = kClick;
            for (int k = 0; k < nImp_; ++k) {
                impT_[k] = t; impA_[k] = a * (0.65f + 0.35f * rng_.uniform()) * ((k & 1) ? -1.0f : 1.0f);
                t += 1 + static_cast<int>((0.5f + rng_.uniform()) * spread * 0.001f * sr_);
                a *= 0.85f;
            }
        }
        nextImp_ = 0; pendNeg_ = 0.0f;
        float malletMs = mapExp(vel, 5.0f, 1.5f) * lerp(1.4f, 0.7f, smoothstep(x, 0.15f, 0.8f));
        // A mallet longer than half a period cannot excite the body; shorten it for high tunes.
        const float halfPeriodMs = 500.0f / f0_;
        if (malletMs > halfPeriodMs) malletMs = halfPeriodMs;
        malletLen_ = static_cast<int>(malletMs * 0.001f * sr_); if (malletLen_ < 2) malletLen_ = 2;
        malletInv_ = 1.0f / static_cast<float>(malletLen_);
        // A pulse of area A drives a resonator like A impulses: normalise the bump by its area.
        malletGain_ = kMallet / (0.6366f * static_cast<float>(malletLen_));
        float tNoise = 0.0f;
        if (wNoise_ > 0.0f) {
            const float u = smoothstep(x, 0.45f, 1.0f);
            // The burst darkens for low bodies (a brush on a big drum), so it can still excite them.
            const float fN = mapExp(u, 900.0f, 9000.0f) * (1.0f + 0.5f * p.velToTone * (vel - 0.7f)) * clamp(f0_ / 300.0f, 0.3f, 1.0f);
            noiseBp_.set(fN, lerp(0.7f, 1.8f, u), sr_); noiseBp_.reset();
            tNoise = mapExp(u, 2.0f, 60.0f) * (0.6f + 0.4f * vel);
            const float cap = T > 2.0f ? T : 2.0f;
            if (tNoise > cap) tNoise = cap;
            noiseEnv_.setDecayMs(tNoise, sr_); noiseEnv_.trigger(1.0f);
        } else {
            noiseEnv_.kill();
        }

        // Body
        const float body = m_[MacroBody];
        const float c = smoothstep(body, 0.2f, 0.5f);
        wMem_ = std::sqrt(1.0f - c);
        wMod_ = std::sqrt(c);
        inc_ = f0_ / sr_;
        const float bodyLow = clamp(body / 0.4f, 0.0f, 1.0f);
        const float bendSemis = lerp(24.0f, 6.0f, bodyLow) * (0.7f + 0.3f * vel);
        bendRatio_ = std::exp2(bendSemis / 12.0f) - 1.0f;
        bend_.setDecayMs(mapExp(m_[MacroTune], 45.0f, 6.0f), sr_); bend_.trigger(1.0f);
        memK_ = decayCoef(T, sr_);
        memAmp_ = 0.0f; phase_ = 0.0f;
        triMix_ = 0.6f * bodyLow;

        nModes_ = wMod_ > 0.0f ? kModes : 0;
        if (nModes_ > 0) {
            static const float H[kModes] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
            static const float Hd[kModes] = { 1.0f, 0.6f, 0.4f, 0.3f, 0.22f, 0.17f };
            static const float Hg[kModes] = { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f };
            static const float M[kModes] = { 1.0f, 1.48f, 2.41f, 3.62f, 5.31f, 7.48f };
            static const float Md[kModes] = { 1.0f, 0.9f, 0.8f, 0.7f, 0.55f, 0.45f };
            static const float Mg[kModes] = { 0.8f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f };
            const float t = smoothstep(body, 0.4f, 1.0f);
            const float hat = smoothstep(body, 0.85f, 1.0f);
            float sum = 0.0f;
            for (int k = 0; k < kModes; ++k) {
                const float r = std::exp2(lerp(std::log2(H[k]), std::log2(M[k]), t));
                const float spread = 1.0f + 0.06f * hat * rng2_.bipolar();
                const float d = lerp(Hd[k], Md[k], t);
                float g = lerp(Hg[k], Mg[k], t);
                g = lerp(g, 0.8f, hat);
                g *= 1.0f + p.velToTone * (vel - 0.7f) * 0.5f * std::log2(r);
                ratio_[k] = r;
                res_[k].set(f0_ * r * spread, T * 0.001f * d, sr_); res_[k].reset();
                gain_[k] = g > 0.0f ? g : 0.0f;
                sum += gain_[k];
            }
            const float norm = sum * 0.6f;
            if (norm > 1.0f) for (int k = 0; k < kModes; ++k) gain_[k] /= norm;
            for (int k = 0; k < kModes; ++k) gain_[k] *= kModal;
        } else {
            for (auto& r : res_) r.reset();
        }
        bodyFollow_.setTimeMs(5.0f, sr_); bodyFollow_.reset(0.0f); bodyEnv_ = 0.0f; fbAgc_ = 0.35f;

        // Grit
        gritAmt_ = clamp(texture, 0.0f, 1.0f) * (0.25f + 0.75f * x);
        if (gritAmt_ > 0.0f) {
            gritP_ = mapExp(x, 400.0f, 6000.0f) / sr_;
            gritLp_.setLp(mapExp(x, 3000.0f, 12000.0f), sr_); gritLp_.reset();
            gritRedux_.set(8 + static_cast<int>(4.0f * (1.0f - texture)), 1, 0.5f); gritRedux_.reset();
            gritEnv_.setDecayMs(T < 150.0f ? T : 150.0f, sr_); gritEnv_.trigger(1.0f);
        } else {
            gritEnv_.kill();
        }
        gritPend_ = 0.0f;

        // Shaper
        const float B = m_[MacroBreak];
        shape_ = B >= 0.001f && !forceBypass_;
        foldG_ = 1.0f + 6.0f * B * B;
        satG_ = 1.0f + 3.0f * B;
        satNorm_ = 1.0f / fastTanh(satG_);
        shapeMix_ = smoothstep(B, 0.0f, 0.25f);
        tone_.set(clamp(24.0f * f0_, 1800.0f, 16000.0f), 0.5f, sr_); tone_.reset();

        // Feedback
        fbAmt_ = 0.85f * smoothstep(B, 0.5f, 1.0f);
        if (forceBypass_) fbAmt_ = 0.0f;
        fbHp_.setLp(30.0f, sr_); fbHp_.reset();
        fbLp_.setLp(clamp(6.0f * f0_, 400.0f, 8000.0f), sr_); fbLp_.reset();
        fbEnv_.setDecayMs(1.5f * T, sr_); fbEnv_.trigger(fbAmt_ > 0.0f ? 1.0f : 0.0f);
        prevOut_ = 0.0f;

        pos_ = 0;
        const float lifeMs = 3.2f * T + tNoise + 30.0f;
        maxLife_ = static_cast<int>((lifeMs < 6000.0f ? lifeMs : 6000.0f) * 0.001f * sr_);
        active_ = true;
    }

    void render(float* out, int n) {
        if (!active_) { for (int i = 0; i < n; ++i) out[i] = 0.0f; return; }
        int i = 0;
        for (; i < n; ++i) {
            // Exciter
            float click = pendNeg_; pendNeg_ = 0.0f;
            while (nextImp_ < nImp_ && impT_[nextImp_] == pos_) { click += impA_[nextImp_]; pendNeg_ -= 0.5f * impA_[nextImp_]; ++nextImp_; }
            const float mallet = pos_ < malletLen_ ? fastSin01(0.5f * static_cast<float>(pos_) * malletInv_) : 0.0f;
            const float noise = noiseEnv_.active() ? noiseBp_.bp(rng_.bipolar()) * noiseEnv_.tick() : 0.0f;
            const float exc = excAmp_ * (wClick_ * click + wMallet_ * malletGain_ * mallet + wNoise_ * noiseGain_ * noise);

            // Feedback injection (one-sample delay through prevOut_)
            float fbSig = 0.0f, fbInject = 0.0f;
            const bool fbLive = fbAmt_ > 0.0f && fbEnv_.active();
            if (fbLive) {
                fbSig = fbLp_.lp(fbHp_.hp(prevOut_)) * fbAmt_ * fbEnv_.tick();
                fbInject = fbSig * fbAgc_;
            }
            const float excIn = exc + fbInject;

            // Membrane
            float mem = 0.0f;
            if (wMem_ > 0.0f && (memAmp_ > 0.0f || excIn != 0.0f)) {
                memAmp_ = memAmp_ * memK_ + std::fabs(excIn) * kMemInj;
                if (memAmp_ > kMemCap) memAmp_ = kMemCap;
                const float b = bend_.tick();
                phase_ += inc_ * (1.0f + bendRatio_ * b) + fbSig * 0.15f * fbAmt_;
                phase_ -= std::floor(phase_);
                const float s = fastSin01(phase_);
                const float tri = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
                mem = memAmp_ * (s + triMix_ * (tri - s));
            }

            // Modes
            float modal = 0.0f;
            for (int k = 0; k < nModes_; ++k) modal += gain_[k] * res_[k].tick(excIn);

            const float body = wMem_ * mem + wMod_ * modal;
            if (fbAmt_ > 0.0f) bodyEnv_ = bodyFollow_.lp(std::fabs(body));

            // Grit
            float g = 0.0f;
            if (gritEnv_.active()) {
                float imp = gritPend_; gritPend_ = 0.0f;
                if (rng2_.uniform() < gritP_) {
                    const float a = (0.5f + 0.5f * rng2_.uniform()) * (rng2_.chance(0.5f) ? 1.0f : -1.0f);
                    imp += a; gritPend_ = -0.6f * a;
                }
                g = gritRedux_.tick(gritLp_.lp(imp)) * gritEnv_.tick() * gritAmt_ * 0.7f;
            }

            // Shaper
            const float xin = body + g;
            float y;
            if (shape_) {
                y = foldTri(xin * foldG_);
                y = fastTanh(y * satG_) * satNorm_;
                y = xin + (y - xin) * shapeMix_;
            } else {
                y = xin;
            }
            y = tone_.lp(y);
            prevOut_ = y;
            out[i] = y;

            ++pos_;
            // Hygiene at fixed sample positions (never at block edges), so the
            // result is bit-identical whatever the block size, feedback included.
            if ((pos_ & (kGuardInterval - 1)) == 0) {
                if (fbAmt_ > 0.0f) { for (int k = 0; k < nModes_; ++k) res_[k].limit(4.0f); fbAgc_ = 0.35f / (1.0f + 2.0f * bodyEnv_); }
                memAmp_ = flushDenormal(memAmp_);
                bodyEnv_ = flushDenormal(bodyEnv_);
                prevOut_ = flushDenormal(prevOut_);
                fbHp_.flush(); fbLp_.flush(); gritLp_.flush(); bodyFollow_.flush(); tone_.flush(); noiseBp_.flush();
                bool modesLive = false;
                for (int k = 0; k < nModes_; ++k) { if (res_[k].quiet()) res_[k].reset(); else modesLive = true; }
                const bool live = nextImp_ < nImp_ || pos_ < malletLen_ || noiseEnv_.active() || memAmp_ > 1e-5f
                               || modesLive || gritEnv_.active() || (fbAmt_ > 0.0f && fbEnv_.active());
                if (!live || pos_ >= maxLife_) { active_ = false; ++i; break; }
            }
        }
        for (; i < n; ++i) out[i] = 0.0f;
    }

    bool isActive() const { return active_; }
    const float* effectiveMacros() const { return m_; }
    float fundamentalHz() const { return f0_; }
    float modeRatio(int k) const { return ratio_[k]; }
    /// Test hook: force the shaper and feedback off regardless of Break.
    void setShaperForTest(bool enabled) { forceBypass_ = !enabled; }

private:
    static constexpr float kClick = 1.0f, kMallet = 1.5f, kNoise = 0.25f, kMemInj = 0.6f, kMemCap = 1.2f, kModal = 1.2f;

    float sr_ = 48000.0f, f0_ = 100.0f;
    float m_[kNumMacros] = {};
    int pos_ = 0, maxLife_ = 0;
    bool active_ = false, forceBypass_ = false;
    // exciter
    float wClick_ = 0.0f, wMallet_ = 0.0f, wNoise_ = 0.0f, excAmp_ = 1.0f, malletGain_ = 1.0f, noiseGain_ = 1.0f;
    int impT_[kMaxImpulses] = {}; float impA_[kMaxImpulses] = {}; int nImp_ = 0, nextImp_ = 0; float pendNeg_ = 0.0f;
    int malletLen_ = 2; float malletInv_ = 0.5f;
    ExpDecay noiseEnv_; TptSvf noiseBp_;
    // body
    float wMem_ = 0.0f, wMod_ = 0.0f, triMix_ = 0.0f, memAmp_ = 0.0f, memK_ = 0.0f;
    float phase_ = 0.0f, inc_ = 0.0f, bendRatio_ = 0.0f; ExpDecay bend_;
    Resonator res_[kModes]; float gain_[kModes] = {}; float ratio_[kModes] = {}; int nModes_ = 0;
    // grit
    float gritAmt_ = 0.0f, gritP_ = 0.0f, gritPend_ = 0.0f; ExpDecay gritEnv_; OnePole gritLp_; Redux gritRedux_;
    // shaper + feedback
    bool shape_ = false; float foldG_ = 1.0f, satG_ = 1.0f, satNorm_ = 1.0f, shapeMix_ = 0.0f;
    float fbAmt_ = 0.0f, prevOut_ = 0.0f, bodyEnv_ = 0.0f, fbAgc_ = 0.35f; ExpDecay fbEnv_; OnePole fbHp_, fbLp_, bodyFollow_;
    TptSvf tone_;
    Rng rng_, rng2_;
};

} // namespace hic
