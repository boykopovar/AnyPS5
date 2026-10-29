#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>

static constexpr int SCE_OK = 0;

static constexpr int GUEST_CLOCK_REALTIME = 0;
static constexpr int GUEST_CLOCK_VIRTUAL = 1;
static constexpr int GUEST_CLOCK_PROF = 2;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_REALTIME_PRECISE = 9;
static constexpr int GUEST_CLOCK_SECOND = 13;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t BURN_NANOS = 200 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t IDLE_LIMIT_NANOS = 100 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t GIVE_UP_NANOS = 10 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

static void SetTimeZone(const char* zone) {
#ifdef _WIN32
    Require(_putenv_s("TZ", zone) == 0);
    _tzset();
#else
    Require(setenv("TZ", zone, 1) == 0);
    tzset();
#endif
}

static bool OmittedResolutionSucceeds() {
    constexpr int clocks[] = {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 3, 6, 9999, -1, INT_MIN, INT_MAX};
    for (const int clock : clocks) {
        *__error_nid_postfix() = 77;
        try {
            if (clock_getres_nid_postfix(clock, nullptr) == SCE_OK && *__error_nid_postfix() == 77) continue;
        } catch (const std::runtime_error&) {
        }
        std::fprintf(stderr, "clock_getres(%d, nullptr) must succeed without changing errno\n", clock);
        return false;
    }
    return true;
}

static void UnsupportedResolutionStillThrows() {
    KernelTimespec resolution{-1, -1};
    bool rejected = false;
    try {
        clock_getres_nid_postfix(3, &resolution);

    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected && resolution.tv_sec == -1 && resolution.tv_nsec == -1);
    constexpr int fault = static_cast<int>(0x8002000eu);
    Require(sceKernelClockGetres(GUEST_CLOCK_REALTIME, nullptr) == fault);
    Require(sceKernelClockGetres(3, nullptr) == fault);
}

static void OmittedTimeIsFaultAfterClockValidation() {
    for (const int clockId : {GUEST_CLOCK_REALTIME, GUEST_CLOCK_VIRTUAL, GUEST_CLOCK_PROF, GUEST_CLOCK_MONOTONIC,
             GUEST_CLOCK_REALTIME_PRECISE, GUEST_CLOCK_SECOND, GUEST_CLOCK_THREAD_CPUTIME_ID, GUEST_CLOCK_PROCESS_CPUTIME_ID}) {
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, nullptr) == -1 && *__error_nid_postfix() == 14);
    }
    bool rejected = false;
    try {
        clock_gettime_nid_postfix(3, nullptr);

    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
}

static KernelTimespec Read(int clockId) {
    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(clockId, &time) == SCE_OK);
    Require(time.tv_sec >= 0);
    Require(time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    return time;
}

static std::int64_t Nanos(int clockId) {
    const KernelTimespec time = Read(clockId);
    return time.tv_sec * NANOS_PER_SECOND + time.tv_nsec;
}

static std::int64_t ResolutionNanos(int clockId) {
    KernelTimespec resolution{-1, -1};
    Require(clock_getres_nid_postfix(clockId, &resolution) == SCE_OK);
    Require(resolution.tv_sec == 0);
    Require(resolution.tv_nsec > 0 && resolution.tv_nsec < NANOS_PER_SECOND);
    KernelTimespec sceResolution{-1, -1};
    Require(sceKernelClockGetres(clockId, &sceResolution) == SCE_OK);
    Require(sceResolution.tv_sec == resolution.tv_sec && sceResolution.tv_nsec == resolution.tv_nsec);
    return resolution.tv_nsec;
}

static void RequireNeverDecreases(int clockId) {
    std::int64_t previous = Nanos(clockId);
    for (int i = 0; i < 10000; ++i) {
        const std::int64_t current = Nanos(clockId);
        Require(current >= previous);
        previous = current;
    }
}

static void SecondClockReportsWholeSeconds() {
    KernelTimespec resolution{-1, -1};
    Require(clock_getres_nid_postfix(GUEST_CLOCK_SECOND, &resolution) == SCE_OK);
    Require(resolution.tv_sec == 1 && resolution.tv_nsec == 0);
    resolution = {-1, -1};
    Require(sceKernelClockGetres(GUEST_CLOCK_SECOND, &resolution) == SCE_OK);
    Require(resolution.tv_sec == 1 && resolution.tv_nsec == 0);

    KernelTimespec sceSecond{-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_SECOND, &sceSecond) == SCE_OK);
    Require(sceSecond.tv_sec > 0 && sceSecond.tv_nsec == 0);

    std::int64_t previous = 0;
    for (int i = 0; i < 1000; ++i) {
        const KernelTimespec before = Read(GUEST_CLOCK_REALTIME);
        const KernelTimespec second = Read(GUEST_CLOCK_SECOND);
        const KernelTimespec after = Read(GUEST_CLOCK_REALTIME);
        Require(second.tv_nsec == 0);
        Require(second.tv_sec >= before.tv_sec - 1 && second.tv_sec <= after.tv_sec);
        Require(second.tv_sec >= previous);
        previous = second.tv_sec;
    }
}

static void ProcessClocksAreOrdered() {
    const std::int64_t profResolution = ResolutionNanos(GUEST_CLOCK_PROF);
    Require(ResolutionNanos(GUEST_CLOCK_VIRTUAL) == profResolution);
    for (int i = 0; i < 1000; ++i) {
        const std::int64_t user = Nanos(GUEST_CLOCK_VIRTUAL);
        const std::int64_t userAndSystem = Nanos(GUEST_CLOCK_PROF);
        const std::int64_t execution = Nanos(GUEST_CLOCK_PROCESS_CPUTIME_ID);
        Require(user <= userAndSystem);
        Require(userAndSystem <= execution + profResolution);
    }
}

static void BusyProcessAccumulatesUserTime() {
    const std::int64_t wallStart = Nanos(GUEST_CLOCK_MONOTONIC);
    const std::int64_t profStart = Nanos(GUEST_CLOCK_PROF);
    const std::int64_t userStart = Nanos(GUEST_CLOCK_VIRTUAL);
    std::int64_t user = userStart;
    volatile std::uint64_t work = 0;
    while (user - userStart < BURN_NANOS) {
        Require(Nanos(GUEST_CLOCK_MONOTONIC) - wallStart < GIVE_UP_NANOS);
        for (int i = 0; i < 1000000; ++i) work = work + 1;
        const std::int64_t current = Nanos(GUEST_CLOCK_VIRTUAL);
        Require(current >= user);
        user = current;
    }
    Require(Nanos(GUEST_CLOCK_PROF) - profStart >= user - userStart);
}

static void IdleProcessAccumulatesNone() {
    const std::int64_t userStart = Nanos(GUEST_CLOCK_VIRTUAL);
    const std::int64_t profStart = Nanos(GUEST_CLOCK_PROF);
    Require(sceKernelUsleep_nid_postfix(200000) == SCE_OK);
    Require(Nanos(GUEST_CLOCK_VIRTUAL) - userStart <= IDLE_LIMIT_NANOS);
    Require(Nanos(GUEST_CLOCK_PROF) - profStart <= IDLE_LIMIT_NANOS);
}

static std::int64_t Microseconds() {
    KernelTimeval time{-1, -1};
    Require(gettimeofday_nid_postfix(&time, nullptr) == SCE_OK);
    return time.tv_sec * 1000000LL + time.tv_usec;
}

static void XtimeTicksAreGettimeofdayMicroseconds() {
    for (int i = 0; i < 100; ++i) {
        const std::int64_t before = Microseconds();
        const std::int64_t ticks = _Xtime_get_ticks_nid_postfix();
        const std::int64_t after = Microseconds();
        Require(before <= ticks && ticks <= after);
    }
}

static void CalendarConversionsInitializeTheirState() {
    SetTimeZone("UTC0");
    constexpr std::uint64_t sentinel = 0xa5a5a5a5a5a5a5a5ull;
    struct GuardedState {
        std::uint64_t before;
        KernelTimesec state;
        std::uint64_t after;
    };
    struct GuardedDst {
        std::int32_t seconds;
        std::uint32_t after;
    };
    static_assert(sizeof(GuardedDst) == 8);
    for (const auto seconds : std::array<std::int64_t, 4>{-86400, 0, 1767225600, 2147483648}) {
        GuardedState local{sentinel, {-1, 0xa5a5a5a5u, 0xa5a5a5a5u}, sentinel};
        std::int64_t utc = -1;
        std::int32_t dst = -1;
        Require(sceKernelConvertLocaltimeToUtc(seconds, 0, &utc, &local.state, &dst) == SCE_OK);
        Require(utc == seconds && dst == 0);
        Require(local.state.t == seconds && local.state.west_sec == 0 && local.state.dst_sec == 0);
        Require(local.before == sentinel && local.after == sentinel);
        GuardedState universal{sentinel, {-1, 0xa5a5a5a5u, 0xa5a5a5a5u}, sentinel};
        std::int64_t converted = -1;
        GuardedDst universalDst{-1, 0xa5a5a5a5u};
        Require(sceKernelConvertUtcToLocaltime(utc, &converted, &universal.state, &universalDst.seconds) == SCE_OK);
        Require(converted == seconds && universalDst.seconds == 0);
        Require(universalDst.after == 0xa5a5a5a5u);
        Require(universal.state.t == seconds && universal.state.west_sec == 0 && universal.state.dst_sec == 0);
        Require(universal.before == sentinel && universal.after == sentinel);
    }
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, nullptr) == SCE_OK);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, nullptr) == SCE_OK);
}

static void RequireSettimeError(int clockId, const KernelTimespec* time, int error) {
    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(clockId, time) == -1 && *__error_nid_postfix() == error);
}

static bool SettimeThrows(const KernelTimespec& time, const char* requested) {
    try {
        clock_settime_nid_postfix(GUEST_CLOCK_REALTIME, &time);

    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), requested) != nullptr;
    }
    return false;
}

static void SettimeValidatesLikeFreebsd() {
    const KernelTimespec valid{1767225600, 5};
    RequireSettimeError(GUEST_CLOCK_REALTIME, nullptr, 14);
    RequireSettimeError(GUEST_CLOCK_MONOTONIC, nullptr, 14);
    const auto unreadable = reinterpret_cast<const KernelTimespec*>(1);
    RequireSettimeError(GUEST_CLOCK_REALTIME, unreadable, 14);
    RequireSettimeError(GUEST_CLOCK_MONOTONIC, unreadable, 14);
    for (const int clockId : {GUEST_CLOCK_VIRTUAL, GUEST_CLOCK_PROF, GUEST_CLOCK_MONOTONIC, GUEST_CLOCK_REALTIME_PRECISE,
             GUEST_CLOCK_SECOND, GUEST_CLOCK_THREAD_CPUTIME_ID, GUEST_CLOCK_PROCESS_CPUTIME_ID, -1, 16})
        RequireSettimeError(clockId, &valid, 22);
    const KernelTimespec negativeNanoseconds{0, -1};
    const KernelTimespec wholeSecondOfNanoseconds{0, NANOS_PER_SECOND};
    const KernelTimespec negativeSeconds{-1, 0};
    RequireSettimeError(GUEST_CLOCK_REALTIME, &negativeNanoseconds, 22);
    RequireSettimeError(GUEST_CLOCK_REALTIME, &wholeSecondOfNanoseconds, 22);
    RequireSettimeError(GUEST_CLOCK_REALTIME, &negativeSeconds, 22);
    RequireSettimeError(GUEST_CLOCK_MONOTONIC, &negativeSeconds, 22);
    Require(SettimeThrows(valid, "1767225600.000000005"));
    Require(SettimeThrows(KernelTimespec{0, 0}, "0.000000000"));
    Require(SettimeThrows(KernelTimespec{1, NANOS_PER_SECOND - 1}, "1.999999999"));
}

static std::int32_t RuntimeDaylight(std::int32_t daylight) {
#ifdef _WIN32
    long bias = 0;
    Require(_get_dstbias(&bias) == 0);
    return daylight == 0 ? 0 : static_cast<std::int32_t>(-bias);
#else
    return daylight;
#endif
}

static void RequireZone(const char* zone, std::int64_t utc, std::int64_t standardEast, std::int32_t zoneDaylight, std::int32_t daylightInEffect) {
    SetTimeZone(zone);
    zoneDaylight = RuntimeDaylight(zoneDaylight);
    const std::int32_t daylight = daylightInEffect == 0 ? 0 : zoneDaylight;
    const std::int64_t east = standardEast + daylight;
    std::int64_t local = -1;
    KernelTimesec universal{-1, 0xa5a5a5a5u, 0xa5a5a5a5u};
    std::int32_t universalDst = -1;
    Require(sceKernelConvertUtcToLocaltime(utc, &local, &universal, &universalDst) == SCE_OK);
    Require(local == utc + east && universalDst == daylight);
    Require(universal.t == utc && universal.west_sec == static_cast<std::uint32_t>(east - daylight) && universal.dst_sec == static_cast<std::uint32_t>(daylight));
    std::int64_t converted = -1;
    KernelTimesec state{-1, 0xa5a5a5a5u, 0xa5a5a5a5u};
    std::int32_t dst = -1;
    Require(sceKernelConvertLocaltimeToUtc(local, 0, &converted, &state, &dst) == SCE_OK);
    Require(converted == utc && dst == zoneDaylight);
    Require(state.t == local && state.west_sec == universal.west_sec && state.dst_sec == universal.dst_sec);
}

static void RequireTimezone(const char* zone, std::int32_t minutesEast, std::int32_t daylightMinutes) {
    SetTimeZone(zone);
    KernelTimezone tz{-1, -1};
    Require(sceKernelGettimezone(&tz) == SCE_OK);
    Require(tz.tz_minuteswest == minutesEast && tz.tz_dsttime == RuntimeDaylight(daylightMinutes * 60) / 60);
}

static void TimezoneReportsTheStandardOffsetAndDaylightAmount() {
    RequireTimezone("UTC0", 0, 0);
    RequireTimezone("IST-5:30", 330, 0);
    RequireTimezone("BRT+3", -180, 0);
    RequireTimezone("EST5EDT", -300, 60);
    RequireTimezone("CET-1CEST", 60, 60);
    Require(sceKernelGettimezone(nullptr) == static_cast<int>(0x8002000e));
    SetTimeZone("UTC0");
}

static void CalendarConversionsFollowTheHostZone() {
    constexpr std::int64_t winter = 1768478400;
    constexpr std::int64_t summer = 1782907200;
    RequireZone("UTC0", summer, 0, 0, 0);
    RequireZone("IST-5:30", winter, 19800, 0, 0);
    RequireZone("BRT+3", summer, -10800, 0, 0);
    RequireZone("EST5EDT", winter, -18000, 3600, 0);
    RequireZone("EST5EDT", summer, -18000, 3600, 3600);
    SetTimeZone("UTC0");
}

static void KernelTimezoneIsUtc() {
    Require(sceKernelGettimezone(nullptr) == static_cast<int>(0x8002000eu));
    KernelTimezone zone{-1, -1};
    Require(sceKernelGettimezone(&zone) == SCE_OK && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
}

int main() {
    if (!OmittedResolutionSucceeds()) return 1;
    KernelTimezoneIsUtc();
    UnsupportedResolutionStillThrows();
    OmittedTimeIsFaultAfterClockValidation();
    SettimeValidatesLikeFreebsd();
    CalendarConversionsInitializeTheirState();
    CalendarConversionsFollowTheHostZone();
    TimezoneReportsTheStandardOffsetAndDaylightAmount();
    SecondClockReportsWholeSeconds();
    KernelTimezone zone{-1, -1};
    Require(gettimeofday_nid_postfix(nullptr, nullptr) == SCE_OK);
    Require(gettimeofday_nid_postfix(nullptr, &zone) == SCE_OK && zone.tz_minuteswest == 0 && zone.tz_dsttime == 0);
    Require(sceKernelGettimeofday(nullptr) == static_cast<int>(0x8002000eu));
    const std::int64_t beforeSce = Microseconds();
    KernelTimeval sceTime{-1, -1};
    Require(sceKernelGettimeofday(&sceTime) == SCE_OK);
    const std::int64_t afterSce = Microseconds();
    const std::int64_t sceMicroseconds = sceTime.tv_sec * 1000000LL + sceTime.tv_usec;
    Require(sceTime.tv_usec >= 0 && sceTime.tv_usec < 1000000 && beforeSce <= sceMicroseconds && sceMicroseconds <= afterSce);
    const int nullResolutionClocks[] = {GUEST_CLOCK_REALTIME, GUEST_CLOCK_VIRTUAL, GUEST_CLOCK_MONOTONIC, GUEST_CLOCK_SECOND, GUEST_CLOCK_PROCESS_CPUTIME_ID};
    for (const int clockId : nullResolutionClocks) Require(clock_getres_nid_postfix(clockId, nullptr) == SCE_OK);

    KernelTimespec time{-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_VIRTUAL, &time) == SCE_OK);
    Require(time.tv_sec >= 0 && time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);
    time = {-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_PROF, &time) == SCE_OK);
    Require(time.tv_sec >= 0 && time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);

    RequireNeverDecreases(GUEST_CLOCK_VIRTUAL);
    RequireNeverDecreases(GUEST_CLOCK_PROF);
    ProcessClocksAreOrdered();
    BusyProcessAccumulatesUserTime();
    IdleProcessAccumulatesNone();
    XtimeTicksAreGettimeofdayMicroseconds();
}
