#pragma once
#include <cstddef>
#include <cstdint>

#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" void LibcSetMonotonicTimeSource_nid_no_patch(std::uint64_t (APS5_VABI* source)());

struct GuestTm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
static_assert(sizeof(GuestTm) == 36);

extern "C" void LibcSetProcessTimeSource_nid_no_patch(std::uint64_t (APS5_VABI* source)());
