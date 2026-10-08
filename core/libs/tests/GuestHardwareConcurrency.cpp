#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdio>
#include <thread>
#include <type_traits>

extern "C" unsigned int APS5_VABI _ZNSt8__sce_v226_Thrd_hardware_concurrencyEv_nid_postfix() noexcept;

static_assert(sizeof(unsigned int) == 4);
static_assert(std::is_same_v<decltype(_ZNSt8__sce_v226_Thrd_hardware_concurrencyEv_nid_postfix()), unsigned int>);
static_assert(noexcept(_ZNSt8__sce_v226_Thrd_hardware_concurrencyEv_nid_postfix()));

int main() {
    const unsigned int expected = std::thread::hardware_concurrency();
    const auto query = _ZNSt8__sce_v226_Thrd_hardware_concurrencyEv_nid_postfix;
    const unsigned int mainResult = query();
    if (mainResult != expected) {
        std::fprintf(stderr, "guest processor hint %u differs from host hint %u\n", mainResult, expected);
        return 1;
    }
    std::array<unsigned int, 2> results{};
    std::array<bool, 2> consistent{true, true};
    std::array<std::thread, 2> workers;
    for (std::size_t index = 0; index < workers.size(); ++index)
        workers[index] = std::thread([&, index] {
            for (unsigned int iteration = 0; iteration < 128; ++iteration) {
                results[index] = query();
                if (results[index] != std::thread::hardware_concurrency()) consistent[index] = false;
            }
        });
    for (auto& worker : workers) worker.join();
    for (std::size_t index = 0; index < results.size(); ++index) {
        if (!consistent[index] || results[index] != mainResult) {
            std::fprintf(stderr, "guest processor hint differs across threads\n");
            return 1;
        }
    }
}
