#pragma once
#include "hic/Config.h"
#include "hic/Kit.h"
#include "hic/Voice.h"

namespace hic {

/// Fixed pool of voices with per-pad polyphony limits and choke groups.
class VoiceAllocator {
public:
    void prepare(float sr) {
        for (auto& v : voices_) v.prepare(sr);
        counter_ = 0;
    }

    /// Starts a hit on `pad`. Returns the voice index used.
    int noteOn(const KitParams& kit, int pad, int note, float vel, uint32_t seed, bool reverse,
               const KeyParams& key, float driftMul, float texture) {
        if (pad < 0 || pad >= kNumPads) return -1;
        const PadParams& p = kit.pads[pad];

        if (p.chokeGroup != 0)
            for (auto& v : voices_)
                if (v.isActive() && kit.pads[v.pad()].chokeGroup == p.chokeGroup) v.choke(4.0f);
        // (fading voices are ignored by the polyphony count below)

        int slot = -1;
        for (int i = 0; i < kNumVoices; ++i) if (!voices_[i].isActive()) { slot = i; break; }

        // Enforce the pad's own polyphony first: steal its oldest voice.
        int samePad = 0, oldestSame = -1; uint32_t oldestSameAge = 0xffffffffu;
        for (int i = 0; i < kNumVoices; ++i) {
            const Voice& v = voices_[i];
            if (v.isActive() && !v.isFading() && v.pad() == pad) {
                ++samePad;
                if (v.age() < oldestSameAge) { oldestSameAge = v.age(); oldestSame = i; }
            }
        }
        const int maxPoly = p.maxPoly < 1 ? 1 : p.maxPoly;
        if (samePad >= maxPoly && oldestSame >= 0) {
            voices_[oldestSame].choke(2.0f);
            if (slot < 0) slot = oldestSame;
        }
        if (slot < 0) {
            uint32_t oldestAge = 0xffffffffu;
            for (int i = 0; i < kNumVoices; ++i)
                if (voices_[i].age() < oldestAge) { oldestAge = voices_[i].age(); slot = i; }
        }
        voices_[slot].setAge(++counter_);
        voices_[slot].trigger(p, pad, vel, note, seed, reverse, key, driftMul, texture);
        return slot;
    }

    void chokeAll(float fadeMs) { for (auto& v : voices_) v.choke(fadeMs); }

    void render(float* dryL, float* dryR, float* revSend, float* freezeTap, float* scratch, int n) {
        for (auto& v : voices_) v.render(dryL, dryR, revSend, freezeTap, scratch, n);
    }

    int activeCount() const { int c = 0; for (const auto& v : voices_) c += v.isActive() ? 1 : 0; return c; }
    int activeCount(int pad) const { int c = 0; for (const auto& v : voices_) c += (v.isActive() && v.pad() == pad) ? 1 : 0; return c; }
    const Voice& voice(int i) const { return voices_[i]; }

private:
    Voice voices_[kNumVoices];
    uint32_t counter_ = 0;
};

} // namespace hic
