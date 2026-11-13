#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include <algorithm>
#include <chrono>
#include <thread>

extern "C" void APS5_VABI _ZNSt8__sce_v215_Thrd_sleep_forEm_nid_postfix(std::uint64_t milliseconds) {
    if (milliseconds == 0) {
        std::this_thread::yield();
        return;
    }
    constexpr std::uint64_t maximumChunk = 86400000;
    while (milliseconds != 0) {
        const auto chunk = std::min(milliseconds, maximumChunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        milliseconds -= chunk;
    }
}
