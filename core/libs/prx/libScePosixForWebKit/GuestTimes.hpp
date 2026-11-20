#pragma once

#include <cstdint>

struct PosixForWebKitTms {
    std::int32_t tms_utime;
    std::int32_t tms_stime;
    std::int32_t tms_cutime;
    std::int32_t tms_cstime;
};

static_assert(sizeof(PosixForWebKitTms) == 16);
