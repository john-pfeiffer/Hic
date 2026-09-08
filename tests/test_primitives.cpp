#include "Harness.h"
#include "Analysis.h"
#include "hic/Math.h"
#include "hic/Rng.h"
#include "hic/Filters.h"
#include "hic/Envelope.h"
#include "hic/DelayLine.h"
#include "hic/Redux.h"
#include "hic/Saturator.h"
#include <vector>

using namespace hic;
using namespace hictest;

static constexpr float kSr = 48000.0f;

TEST(math_fast_tanh_bounded_and_monotonic) {
    float prev = -2.0f;
    for (float x = -6.0f; x <= 6.0f; x += 0.01f) {
        const float y = fastTanh(x);
        CHECK(y >= -1.0f && y <= 1.0f);
        CHECK(y >= prev - 1e-6f);
        prev = y;
    }
    CHECK_NEAR(fastTanh(0.0f), 0.0f, 1e-7f);
    CHECK_NEAR(fastTanh(0.5f), std::tanh(0.5f), 0.01f);
    CHECK_NEAR(fastExp(-1.0f), std::exp(-1.0f), 0.005f);
    CHECK_NEAR(midiToHz(69.0f), 440.0f, 1e-3f);
    CHECK_NEAR(dbToGain(-6.0206f), 0.5f, 1e-4f);
}

TEST(rng_is_deterministic_and_uniform) {
    Rng a(42), b(42);
    for (int i = 0; i < 1000; ++i) CHECK(a.next() == b.next());
    Rng c(42);
    double sum = 0; int n = 100000; float mn = 1.0f, mx = 0.0f;
    for (int i = 0; i < n; ++i) { const float u = c.uniform(); sum += double(u); mn = std::min(mn, u); mx = std::max(mx, u); }
    CHECK_NEAR(sum / n, 0.5, 0.01);
    CHECK(mn >= 0.0f && mx < 1.0f);
    CHECK(hashSeed(1, 2, 3) != hashSeed(1, 3, 2));
    CHECK(hashSeed(1, 2, 3) == hashSeed(1, 2, 3));
    CHECK(hashSeed(0, 0, 0) != 0u);
}

TEST(svf_is_minus_3db_at_cutoff) {
    for (float fc : { 200.0f, 1000.0f, 8000.0f }) {
        TptSvf f; f.set(fc, 0.7071f, kSr);
        // Feed a sine at fc, measure steady-state amplitude.
        float ph = 0.0f; float pk = 0.0f;
        const int n = 48000;
        for (int i = 0; i < n; ++i) {
            const float x = std::sin(ph); ph += kTwoPi * fc / kSr;
            const float y = f.lp(x);
            if (i > n / 2) pk = std::max(pk, std::fabs(y));
        }
        CHECK_NEAR(gainToDb(pk), -3.0f, 0.3f);
    }
}

TEST(onepole_lowpass_settles) {
    OnePole p; p.setLp(100.0f, kSr);
    float y = 0.0f;
    for (int i = 0; i < 48000; ++i) y = p.lp(1.0f);
    CHECK_NEAR(y, 1.0f, 1e-4f);
    CHECK_NEAR(p.hp(1.0f), 0.0f, 1e-3f);
}

TEST(resonator_rings_at_frequency_with_t60) {
    Resonator r; r.set(1000.0f, 0.1f, kSr);
    std::vector<float> out(24000);
    out[0] = r.tick(1.0f);
    for (size_t i = 1; i < out.size(); ++i) out[i] = r.tick(0.0f);
    CHECK(!hasNaN(out.data(), int(out.size())));
    CHECK_NEAR(peak(out.data(), int(out.size())), 1.0f, 0.15f);
    // Count zero crossings in the first 100 ms: 1000 Hz -> ~200 crossings.
    int zc = 0;
    for (int i = 1; i < 4800; ++i) if ((out[size_t(i)] >= 0) != (out[size_t(i - 1)] >= 0)) ++zc;
    CHECK_NEAR(zc, 200, 4);
    // -60 dB at 100 ms.
    CHECK_NEAR(decayMs(out.data(), int(out.size()), kSr), 100.0f, 5.0f);
    Resonator mute; mute.set(30000.0f, 1.0f, kSr);
    CHECK(mute.muted());
}

TEST(exp_decay_reaches_minus_60db_on_time) {
    for (float ms : { 10.0f, 100.0f, 500.0f }) {
        ExpDecay e; e.setDecayMs(ms, kSr); e.trigger(1.0f);
        const int target = int(ms * 0.001f * kSr);
        int i = 0;
        while (e.tick() > 0.001f) ++i;
        CHECK_NEAR(i, target, target * 0.02 + 2);
        CHECK(!e.active() || e.value() <= 1e-3f);
    }
}

TEST(ar_env_gates) {
    ArEnv e; e.setAttackMs(1.0f, kSr); e.setReleaseMs(50.0f, kSr);
    e.gate(true);
    float y = 0.0f;
    for (int i = 0; i < 480; ++i) y = e.tick();   // 10 ms
    CHECK(y > 0.99f);
    e.gate(false);
    for (int i = 0; i < 4800; ++i) y = e.tick();  // 100 ms
    CHECK(y < 0.15f);
    for (int i = 0; i < 48000; ++i) y = e.tick();
    CHECK(y == 0.0f);
    CHECK(!e.active());
}

TEST(delay_line_reads_back) {
    DelayLine<16> d; d.clear();
    for (int i = 0; i < 10; ++i) d.write(float(i));
    CHECK(d.read(0) == 9.0f);
    CHECK(d.read(3) == 6.0f);
    CHECK_NEAR(d.readLin(2.5f), 6.5f, 1e-6f);
}

TEST(redux_reduces_bits_and_rate) {
    Redux r; r.prepare(kSr); r.set(8, 1, 0.0f);
    Rng rng(7);
    const int n = 20000;
    std::vector<float> out(n), scratch(n);
    for (int i = 0; i < n; ++i) out[size_t(i)] = r.tick(rng.bipolar());
    CHECK(distinctValues(out.data(), n, scratch.data()) <= 257);
    CHECK(!hasNaN(out.data(), n));

    Redux r4; r4.prepare(kSr); r4.set(16, 4, 0.0f);
    int holds = 0;
    float prev = -100.0f;
    for (int i = 0; i < n; ++i) { const float y = r4.tick(rng.bipolar()); if (y == prev) ++holds; prev = y; }
    CHECK_NEAR(holds, n * 3 / 4, n / 20);

    Redux clean; clean.prepare(kSr);
    CHECK(clean.bypassed());
    CHECK(clean.tick(0.123f) == 0.123f);
}

TEST(saturator_is_bounded_and_blocks_dc) {
    Saturator s; s.prepare(kSr); s.setDrive(12.0f);
    float mx = 0.0f;
    Rng rng(3);
    for (int i = 0; i < 48000; ++i) mx = std::max(mx, std::fabs(s.tick(rng.bipolar() * 2.0f)));
    CHECK(mx <= 1.5f);
    // Silence in -> silence out once the DC blocker settles.
    float y = 1.0f;
    for (int i = 0; i < 96000; ++i) y = s.tick(0.0f);
    CHECK(std::fabs(y) < 1e-3f);
    Saturator clean; clean.prepare(kSr);
    CHECK(clean.tick(0.3f) == 0.3f);
}

TEST(alloc_guard_catches_heap_use) {
    const int64_t before = hictest::allocViolations();
    {
        // A direct call to ::operator new cannot be elided by the compiler
        // (a new-expression or a libc++ builtin allocation can be).
        hictest::allocForbidden() = true;
        void* p = ::operator new(64);
        CHECK(p != nullptr);
        ::operator delete(p);
        hictest::allocForbidden() = false;
    }
    CHECK(hictest::allocViolations() == before + 1);
    {
        NO_ALLOC_ZONE();
        TptSvf f; f.set(1000.0f, 1.0f, kSr);
        float acc = 0.0f;
        for (int i = 0; i < 1000; ++i) acc += f.lp(1.0f);
        CHECK(acc > 0.0f);
    }
}
