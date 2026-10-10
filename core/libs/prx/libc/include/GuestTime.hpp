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
    std::int64_t tm_gmtoff;
    const char* tm_zone;
};
static_assert(offsetof(GuestTm, tm_gmtoff) == 40);
static_assert(offsetof(GuestTm, tm_zone) == 48);
static_assert(sizeof(GuestTm) == 56);
