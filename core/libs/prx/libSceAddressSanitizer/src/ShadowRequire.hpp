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
    const auto begin = AsanRuntime::ShadowBase + (address >> 3);
    const auto end = AsanRuntime::ShadowBase + ((address + size - 1) >> 3) + 1;
    if (!AsanRuntime::ShadowRangeAvailable_nid_no_patch(begin, std::min(end, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) {
        ShadowUnavailable_nid_no_patch(function);
    }
}
