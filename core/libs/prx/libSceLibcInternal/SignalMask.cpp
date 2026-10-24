#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cstdint>

extern "C" int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet);

namespace {

int ChangeSignalMask(int how, int mask) {
    const std::uint32_t set[4] = {std::bit_cast<std::uint32_t>(mask), 0, 0, 0};
    std::uint32_t previous[4]{};
    const int result = sigprocmask_nid_postfix(how, set, previous);
    return result == 0 ? std::bit_cast<int>(previous[0]) : result;
}

}

extern "C" {

int APS5_VABI sigblock_nid_postfix(int mask) {
    return ChangeSignalMask(1, mask);
}

int APS5_VABI sigsetmask_nid_postfix(int mask) {
    return ChangeSignalMask(3, mask);
}

}
