#pragma once

#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowCheck.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>

inline void RequireShadowRange_nid_no_patch(std::uintptr_t address, std::size_t size, const char* function) {
    if (size == 0) {
        return;
    }
    constexpr std::uintptr_t limit = 1ull << 47;
    if (address >= limit || size > limit - address ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && address + size > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(function);
    }
    const auto begin = AsanRuntime::ShadowBase + (address >> 3);
    const auto end = AsanRuntime::ShadowBase + ((address + size - 1) >> 3) + 1;
    if (!AsanRuntime::ShadowRangeAvailable_nid_no_patch(begin, std::min(end, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) {
        ShadowUnavailable_nid_no_patch(function);
    }
}
