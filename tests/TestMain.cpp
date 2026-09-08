#include "Harness.h"
#include <cstring>

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
