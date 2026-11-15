#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "relative sleep: %s\n", message);
    std::abort();
}

}

int main(int argc, char** argv) {
    if (argc == 2) {
        const auto milliseconds = static_cast<std::uint64_t>(std::strtoull(argv[1], nullptr, 10));
        std::puts("sleeping");
        std::fflush(stdout);
        try {
            _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(milliseconds);
        } catch (...) {
            std::puts("failed");
            std::fflush(stdout);
            return 1;
        }
        std::puts("returned");
        std::fflush(stdout);
        return 0;
    }
    using Clock = std::chrono::steady_clock;
    _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(0);
    for (const std::uint64_t milliseconds : {1, 20, 80}) {
        const auto start = Clock::now();
        _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(milliseconds);
        Require(Clock::now() - start >= std::chrono::milliseconds(milliseconds), "returned before the requested duration");
    }
    std::puts("relative sleep checks passed");
}
