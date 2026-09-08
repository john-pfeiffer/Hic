#include "Harness.h"
#include <new>
#include <cstring>

namespace hictest {

Case*& caseList() { static Case* head = nullptr; return head; }
int& failures() { static int n = 0; return n; }

void addCase(Case* c) {
    // Append so tests run in file order.
    Case** p = &caseList();
    while (*p) p = &(*p)->next;
    *p = c;
}

void reportFailure(const char* file, int line, const char* expr, const char* detail) {
    ++failures();
    std::fprintf(stderr, "  FAIL %s:%d: %s %s\n", file, line, expr, detail);
}

static int64_t g_allocCount = 0;
static bool g_allocForbidden = false;
static int64_t g_allocViolations = 0;

int64_t allocCount() { return g_allocCount; }
bool& allocForbidden() { return g_allocForbidden; }
int64_t& allocViolations() { return g_allocViolations; }

} // namespace hictest

static void* hicAlloc(std::size_t n) {
    ++hictest::g_allocCount;
    if (hictest::g_allocForbidden) ++hictest::g_allocViolations;
    void* p = std::malloc(n ? n : 1);
    if (!p) std::abort();
    return p;
}

void* operator new(std::size_t n) { return hicAlloc(n); }
void* operator new[](std::size_t n) { return hicAlloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0;
    for (hictest::Case* c = hictest::caseList(); c; c = c->next) {
        if (filter && !std::strstr(c->name, filter)) continue;
        const int before = hictest::failures();
        std::printf("[ RUN  ] %s\n", c->name);
        std::fflush(stdout);
        c->fn();
        ++ran;
        std::printf("[ %s ] %s\n", hictest::failures() == before ? " OK " : "FAIL", c->name);
    }
    std::printf("%d test(s) ran, %d failure(s)\n", ran, hictest::failures());
    return hictest::failures() == 0 ? 0 : 1;
}
