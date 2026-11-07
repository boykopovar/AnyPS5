#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

#include <cstdint>

struct AtomicWaitForwards_nid_no_patch {
    int (APS5_VABI* wait32)(std::uint32_t*, std::uint32_t, KernelUseconds*, const char*);
    int (APS5_VABI* wait64)(std::uint64_t*, std::uint64_t, KernelUseconds*, const char*);
    int (APS5_VABI* wake)(void*, std::int32_t);
};

extern "C" void RegisterAtomicWaitForwards_nid_no_patch(const AtomicWaitForwards_nid_no_patch* forwards);
