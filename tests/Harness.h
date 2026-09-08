#pragma once
// Minimal test harness: TEST() registers a case, CHECK() records failures.
// ScopedNoAllocZone turns any heap allocation inside it into a failure,
// which is how the "no allocation in the audio path" rule is enforced.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <chrono>

namespace hictest {

struct Case {
    const char* name;
    void (*fn)();
    Case* next;
};

Case*& caseList();
int& failures();
void addCase(Case* c);
void reportFailure(const char* file, int line, const char* expr, const char* detail);

// Allocation tracking (implemented in HarnessMain.cpp via global operator new).
int64_t allocCount();
bool& allocForbidden();
int64_t& allocViolations();

struct ScopedNoAllocZone {
    const char* file; int line; int64_t before;
    ScopedNoAllocZone(const char* f, int l) : file(f), line(l), before(allocViolations()) { allocForbidden() = true; }
    ~ScopedNoAllocZone() {
        allocForbidden() = false;
        if (allocViolations() != before)
            reportFailure(file, line, "no heap allocation in audio path", "operator new was called");
    }
};

struct Registrar {
    Registrar(Case* c) { addCase(c); }
};

/// Cheap timer for the cycles-per-sample benchmarks.
struct Timer {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    double seconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
};

} // namespace hictest

#define HIC_CAT2(a, b) a##b
#define HIC_CAT(a, b) HIC_CAT2(a, b)

#define TEST(name)                                                              \
    static void HIC_CAT(test_fn_, name)();                                      \
    static hictest::Case HIC_CAT(test_case_, name){#name, &HIC_CAT(test_fn_, name), nullptr}; \
    static hictest::Registrar HIC_CAT(test_reg_, name)(&HIC_CAT(test_case_, name)); \
    static void HIC_CAT(test_fn_, name)()

#define CHECK(expr)                                                             \
    do { if (!(expr)) hictest::reportFailure(__FILE__, __LINE__, #expr, ""); } while (0)

#define CHECK_MSG(expr, ...)                                                    \
    do { if (!(expr)) { char hic_buf_[256]; std::snprintf(hic_buf_, sizeof hic_buf_, __VA_ARGS__); \
         hictest::reportFailure(__FILE__, __LINE__, #expr, hic_buf_); } } while (0)

#define CHECK_NEAR(a, b, tol)                                                   \
    do { const double hic_a_ = (double)(a), hic_b_ = (double)(b);               \
         if (!(std::fabs(hic_a_ - hic_b_) <= (double)(tol))) {                  \
             char hic_buf_[256]; std::snprintf(hic_buf_, sizeof hic_buf_, "%g vs %g (tol %g)", hic_a_, hic_b_, (double)(tol)); \
             hictest::reportFailure(__FILE__, __LINE__, #a " ~= " #b, hic_buf_); } } while (0)

#define NO_ALLOC_ZONE() hictest::ScopedNoAllocZone HIC_CAT(no_alloc_, __LINE__)(__FILE__, __LINE__)
