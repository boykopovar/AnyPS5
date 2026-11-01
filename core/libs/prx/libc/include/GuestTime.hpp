#pragma once
#include <cstddef>
#include <cstdint>

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
