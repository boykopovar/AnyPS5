#pragma once

#include <cstdint>

struct PosixForWebKitTms {
    std::int32_t tms_utime;
    std::int32_t tms_stime;
    std::int32_t tms_cutime;
    std::int32_t tms_cstime;
};

static_assert(sizeof(PosixForWebKitTms) == 16);

constexpr std::int64_t PosixForWebKitClockTicksPerSecond = 128;

constexpr std::int32_t PosixForWebKitClockTicks(std::int64_t seconds, std::int64_t micros) {
    return static_cast<std::int32_t>(seconds * PosixForWebKitClockTicksPerSecond + micros / (1000000 / PosixForWebKitClockTicksPerSecond));
}
