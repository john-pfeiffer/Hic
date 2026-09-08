#pragma once
#include "hic/Config.h"
#include "hic/Math.h"
#include "hic/Filters.h"
#include "hic/DelayLine.h"

namespace hic {

enum class ReverbType : uint8_t { Spring = 0, Room };

struct ReverbParams {
    ReverbType type   = ReverbType::Spring;
    float decaySec    = 0.4f;     // 0.2..1.2
    float dampHz      = 4000.0f;  // 2k..6k
    float predelayMs  = 8.0f;     // 5..20
    float mix         = 1.0f;     // return level
};

/// A deliberately small reverb: predelay, three series allpasses, two
/// recirculating delays with damping, and for the spring type a chain of
/// short allpasses for the characteristic dispersion. There is no hall.
class SpringReverb {
public:
    static constexpr int kPre  = nextPow2(samplesForMs(21.0f));
    static constexpr int kAp   = nextPow2(samplesForMs(8.0f));
    static constexpr int kLoop = nextPow2(samplesForMs(40.0f));
    static constexpr int kDisp = nextPow2(samplesForMs(1.2f));

    void prepare(float sr) {
        sr_ = sr;
        pre_.clear(); for (auto& a : ap_) a.clear(); for (auto& l : loop_) l.clear(); for (auto& d : disp_) d.clear();
        for (auto& d : damp_) d.reset();
        apLen_[0] = ms(2.1f); apLen_[1] = ms(4.7f); apLen_[2] = ms(7.3f);
        loopLen_[0] = ms(29.0f); loopLen_[1] = ms(37.0f);
        for (int i = 0; i < kDispStages; ++i) dispLen_[i] = ms(0.3f + 0.114f * static_cast<float>(i));
        set(ReverbParams{});
    }

    void set(const ReverbParams& p) {
        type_ = p.type;
        mix_ = p.mix;
        preLen_ = clamp(ms(p.predelayMs), 1, kPre - 2);
        const float t60 = clamp(p.decaySec, 0.1f, 2.0f);
        for (int i = 0; i < 2; ++i) fb_[i] = std::pow(10.0f, -3.0f * static_cast<float>(loopLen_[i]) / (t60 * sr_));
        for (auto& d : damp_) d.setLp(clamp(p.dampHz, 500.0f, 12000.0f), sr_);
    }

    /// Mono send in, stereo added to L/R.
    void process(const float* send, float* L, float* R, int n) {
        for (int i = 0; i < n; ++i) {
            pre_.write(send[i]);
            float x = pre_.read(preLen_);
            for (int k = 0; k < 3; ++k) x = allpass(ap_[k], apLen_[k], 0.5f, x);

            float outL = 0.0f, outR = 0.0f;
            for (int k = 0; k < 2; ++k) {
                const float d = loop_[k].read(loopLen_[k]);
                const float fbk = flushDenormal(damp_[k].lp(d) * fb_[k]);
                loop_[k].write(x + fbk);
                // Different taps for L and R give cheap decorrelation.
                const float tapL = loop_[k].read(static_cast<int>(static_cast<float>(loopLen_[k]) * (k == 0 ? 1.0f : 0.93f)) - 1);
                const float tapR = loop_[k].read(static_cast<int>(static_cast<float>(loopLen_[k]) * (k == 0 ? 0.91f : 1.0f)) - 1);
                outL += tapL; outR += tapR;
            }
            if (type_ == ReverbType::Spring) {
                for (int k = 0; k < kDispStages; ++k) outL = allpass(disp_[k], dispLen_[k], 0.6f, outL);
                outR = outR * 0.6f + outL * 0.4f;
            }
            L[i] += outL * 0.5f * mix_;
            R[i] += outR * 0.5f * mix_;
        }
    }

private:
    static constexpr int kDispStages = 8;
    int ms(float m) const { return static_cast<int>(m * 0.001f * sr_); }

    template <int N>
    static float allpass(DelayLine<N>& d, int len, float g, float x) {
        const float y = d.read(len - 1);
        const float v = x - g * y;
        d.write(v);
        return y + g * v;
    }

    float sr_ = 48000.0f, mix_ = 1.0f;
    ReverbType type_ = ReverbType::Spring;
    int preLen_ = 1, apLen_[3] = { 1, 1, 1 }, loopLen_[2] = { 1, 1 }, dispLen_[kDispStages] = {};
    float fb_[2] = { 0.5f, 0.5f };
    DelayLine<kPre> pre_;
    DelayLine<kAp> ap_[3];
    DelayLine<kLoop> loop_[2];
    DelayLine<kDisp> disp_[kDispStages];
    OnePole damp_[2];
};

} // namespace hic
