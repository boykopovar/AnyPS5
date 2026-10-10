#include "prx/libkernel/Time/include/Time.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main(int argc, char** argv) {
    constexpr std::uint64_t minuteMicros = 60ULL * 1000 * 1000;
    if (argc > 1 && std::strcmp(argv[1], "counter") == 0) {
        Require(sceKernelGetProcessTimeCounter() < minuteMicros * 1000, "first process time counter wrapped");
        return 0;
    }
    const std::uint64_t first = sceKernelGetProcessTime();
    Require(first < minuteMicros, "first process time wrapped");
    Require(sceKernelGetProcessTime() >= first, "process time went backwards");
    return 0;
}
