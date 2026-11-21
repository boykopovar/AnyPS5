extern "C" {
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include <stdexcept>
#include <string>
#include <system_error>
#include "prx/libc/include/GuestCpu.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#ifdef __linux__
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <sched.h>
#include <sys/uio.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#endif
#endif

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EIO = 5;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;

static bool MeasuredInvalidClock(int clockId) {
    return clockId == 3 || clockId == 6 || (clockId >= 22 && clockId < 32);
}

static bool MeasuredIoErrorClock(int clockId) {
    return clockId >= 16 && clockId <= 19;
}

int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp) {
    if (!IsCpuClock(clockId) && (clockId < 0 || clockId >= 32 || MeasuredInvalidClock(clockId))) {
        *__error_nid_postfix() = GUEST_EINVAL;
        return -1;
    }
    if (MeasuredIoErrorClock(clockId)) {
        *__error_nid_postfix() = GUEST_EIO;
        return -1;
    }
    KernelTimespec value{};
    if (ReadClock(clockId, &value) != 0) return -1;
    if (tp == nullptr) {
        *__error_nid_postfix() = GUEST_EFAULT;
        return -1;
    }
    *tp = value;
    return 0;
}

int APS5_VABI gettimeofday_nid_postfix(KernelTimeval* output, KernelTimezone* zoneOutput) {
    KernelTimeval current{};
    auto* tv = &current;
    KernelTimezone zone{};
#ifdef _WIN32
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    std::uint64_t t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    tv->tv_sec = static_cast<std::int64_t>(t / 10000000ULL);
    tv->tv_usec = static_cast<std::int64_t>((t % 10000000ULL) / 10ULL);
#else
    struct timeval native{};
    if (gettimeofday(&native, nullptr) != 0) {
        return -1;
    }
    tv->tv_sec = static_cast<std::int64_t>(native.tv_sec);
    tv->tv_usec = static_cast<std::int64_t>(native.tv_usec);
#endif
    if (output != nullptr && !GuestCopyOut_nid_no_patch(output, &current, sizeof(current))) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    if (zoneOutput != nullptr && !GuestCopyOut_nid_no_patch(zoneOutput, &zone, sizeof(zone))) {
        *__error_nid_postfix() = 14;
        return -1;
    }
    return 0;
}

int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res) {
    if (res == nullptr) return 0;
    if (IsCpuClock(clockId)) clockId = GUEST_CLOCK_THREAD_CPUTIME_ID;
    switch (clockId) {
        case 0:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            *res = {0, 81};
            return 0;
        case 1:
        case 2:
            *res = {0, 1000000};
            return 0;
        case 13:
            *res = {1, 0};
            return 0;
        case GUEST_CLOCK_THREAD_CPUTIME_ID:
        case 21:
            *res = {0, 1000};
            return 0;
        default:
            *__error_nid_postfix() = GUEST_EINVAL;
            return -1;
    }
}

int APS5_VABI clock_settime_nid_postfix(int clockId, const KernelTimespec* tp) {
    KernelTimespec time{};
    if (tp == nullptr || !ReadGuestTime(&time, tp, sizeof(time))) {
        *__error_nid_postfix() = GUEST_EFAULT;
        return -1;
    }
    static_cast<void>(clockId);
    *__error_nid_postfix() = GUEST_EPERM;
    return -1;
}

int SetTimeOfDay_nid_no_patch(const KernelTimeval* tv, const KernelTimezone* tz) {
    KernelTimeval time{};
    KernelTimezone zone{};
    if (tv != nullptr && !ReadGuestTime(&time, tv, sizeof(time))) {
        *__error_nid_postfix() = GUEST_EFAULT;
        return -1;
    }
    if (tz != nullptr && !ReadGuestTime(&zone, tz, sizeof(zone))) {
        *__error_nid_postfix() = GUEST_EFAULT;
        return -1;
    }
    *__error_nid_postfix() = GUEST_EPERM;
    return -1;
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

static constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000e);

using HostZone = SonyHostTimeZone;

static HostZone HostZoneAt(std::int64_t utcTime) {
    return SonyHostZoneAt_nid_no_patch(utcTime);
}

static std::int64_t HostDaylightAmount(std::int64_t utcTime) {
    return SonyHostDaylightAmount_nid_no_patch(utcTime);
}

static KernelTimesec ZoneState(std::int64_t time, const HostZone& zone) {
    return {time, static_cast<std::uint32_t>(zone.standardEast), static_cast<std::uint32_t>(zone.daylight)};
}

int APS5_VABI sceKernelClockGetres(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    if (clock_getres_nid_postfix(static_cast<int>(clock_id), tp) != 0) return SceKernelError(*__error_nid_postfix());
    return 0;
}

int APS5_VABI sceKernelClockGettime(KernelClockid clock_id, KernelTimespec* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    if (clock_gettime_nid_postfix(static_cast<int>(clock_id), tp) != 0) return SceKernelError(*__error_nid_postfix());
    return 0;
}

int APS5_VABI sceKernelConvertLocaltimeToUtc(int64_t local_time, int64_t reserved, int64_t* utc_time, KernelTimesec* timezone, int32_t* dst_seconds) {
    (void)reserved;
    const HostZone guess = HostZoneAt(local_time);
    const HostZone zone = HostZoneAt(local_time - guess.standardEast - guess.daylight);
    if (utc_time != nullptr) {
        const int64_t converted = local_time - zone.standardEast - zone.daylight;
        if (!GuestCopyOut_nid_no_patch(utc_time, &converted, sizeof(converted))) return SceKernelErrorEfault;
    }
    if (timezone != nullptr) {
        const KernelTimesec state = ZoneState(local_time, zone);
        if (!GuestCopyOut_nid_no_patch(timezone, &state, sizeof(state))) return SceKernelErrorEfault;
    }
    if (dst_seconds != nullptr) {
        const int32_t daylight = static_cast<int32_t>(HostDaylightAmount(local_time - zone.standardEast - zone.daylight));
        if (!GuestCopyOut_nid_no_patch(dst_seconds, &daylight, sizeof(daylight))) return SceKernelErrorEfault;
    }
    return 0;
}

int APS5_VABI sceKernelConvertUtcToLocaltime(int64_t utc_time, int64_t* local_time, KernelTimesec* st, int32_t* dst_sec) {
    const HostZone zone = HostZoneAt(utc_time);
    if (local_time != nullptr) {
        const int64_t converted = utc_time + zone.standardEast + zone.daylight;
        if (!GuestCopyOut_nid_no_patch(local_time, &converted, sizeof(converted))) return SceKernelErrorEfault;
    }
    if (st != nullptr) {
        const KernelTimesec state = ZoneState(utc_time, zone);
        if (!GuestCopyOut_nid_no_patch(st, &state, sizeof(state))) return SceKernelErrorEfault;
    }
    if (dst_sec != nullptr) {
        const int32_t daylight = static_cast<int32_t>(zone.daylight);
        if (!GuestCopyOut_nid_no_patch(dst_sec, &daylight, sizeof(daylight))) return SceKernelErrorEfault;
    }
    return 0;
}

int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp) {
    if (tp == nullptr) return SceKernelErrorEfault;
    const int result = gettimeofday_nid_postfix(tp, nullptr);
    return result != 0 && *__error_nid_postfix() == 14 ? SceKernelErrorEfault : result;
}

namespace {

std::mutex guestTimezoneMutex;
bool guestTimezoneStored = false;
KernelTimezone guestTimezone{};

}

void SetGuestTimezone_nid_no_patch(std::int32_t minutesWest, std::int32_t daylightTime) {
    std::lock_guard lock(guestTimezoneMutex);
    guestTimezone = KernelTimezone{minutesWest, daylightTime};
    guestTimezoneStored = true;
}

void ResetGuestTimezone_nid_no_patch() {
    std::lock_guard lock(guestTimezoneMutex);
    guestTimezoneStored = false;
    guestTimezone = KernelTimezone{};
}

int APS5_VABI sceKernelGettimezone(KernelTimezone* tz) {
    if (tz == nullptr) return 0;
    KernelTimezone zone{};
    bool stored = false;
    {
        std::lock_guard lock(guestTimezoneMutex);
        stored = guestTimezoneStored;
        zone = guestTimezone;
    }
    if (!stored) {
        const auto now = static_cast<std::int64_t>(std::time(nullptr));
        zone = KernelTimezone{static_cast<std::int32_t>(HostZoneAt(now).standardEast / 60), static_cast<std::int32_t>(HostDaylightAmount(now) / 60)};
    }
    return GuestCopyOut_nid_no_patch(tz, &zone, sizeof(zone)) ? 0 : SceKernelErrorEfault;
}

uint64_t APS5_VABI sceKernelReadTsc(void) {
    return GuestCpu::ReadTsc();
}

std::uint64_t APS5_VABI HostTscFrequency_nid_no_patch() {
    static const std::uint64_t frequency = [] {
        const CurrentCpuPin pin;
        const std::uint64_t startNanos = RawMonotonicNanos();
        const std::uint64_t startTicks = GuestCpu::ReadTsc();
        TimedWait::SleepNanos(20000000ULL);
        const std::uint64_t elapsedNanos = RawMonotonicNanos() - startNanos;
        const std::uint64_t elapsedTicks = GuestCpu::ReadTsc() - startTicks;
        return static_cast<std::uint64_t>(static_cast<long double>(elapsedTicks) * 1000000000.0L / static_cast<long double>(elapsedNanos));
    }();
    return frequency;
}

uint64_t APS5_VABI sceKernelGetTscFrequency(void) {
    static const std::uint64_t frequency = static_cast<std::uint64_t>(static_cast<double>(HostTscFrequency_nid_no_patch()) / TimeScale());
    return frequency;
}

std::int64_t APS5_VABI sceKernelGetCpuFrequency(void) {
    return static_cast<std::int64_t>(sceKernelGetTscFrequency());
}

unsigned int APS5_VABI sceKernelSleep(unsigned int seconds) {
    SleepUntilComplete(static_cast<std::uint64_t>(seconds) * 1000000000ULL);
    return 0;
}

}

namespace {
const bool clockResolutionRegistered = (RegisterClockGetres_nid_no_patch(&sceKernelClockGetres), true);
}
