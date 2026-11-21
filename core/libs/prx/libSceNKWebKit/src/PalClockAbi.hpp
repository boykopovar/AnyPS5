#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::PalAbi {

struct Clock;

struct ClockVtable {
    void (APS5_VABI* completeDestructor)(Clock*);
    void (APS5_VABI* deletingDestructor)(Clock*);
    void (APS5_VABI* setCurrentTime)(Clock*, double);
    double (APS5_VABI* currentTime)(const Clock*);
    void (APS5_VABI* setPlayRate)(Clock*, double);
    double (APS5_VABI* playRate)(const Clock*);
    void (APS5_VABI* start)(Clock*);
    void (APS5_VABI* stop)(Clock*);
    bool (APS5_VABI* isRunning)(const Clock*);
};

struct Clock {
    const ClockVtable* vptr;
    std::uint8_t running;
    double rate;
    double offset;
    double startTime;
    double lastTime;
};

static_assert(sizeof(ClockVtable) == 72);
static_assert(sizeof(Clock) == 48);
static_assert(alignof(Clock) == 8);
static_assert(offsetof(Clock, vptr) == 0);
static_assert(offsetof(Clock, running) == 8);
static_assert(offsetof(Clock, rate) == 16);
static_assert(offsetof(Clock, offset) == 24);
static_assert(offsetof(Clock, startTime) == 32);
static_assert(offsetof(Clock, lastTime) == 40);
static_assert(std::is_trivially_copyable_v<Clock>);

}  // namespace NKWebKit::PalAbi
