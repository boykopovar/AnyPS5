#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int APS5_VABI clock_getres_nid_postfix(int clockId, KernelTimespec* res);
int APS5_VABI sceKernelClockGettime(KernelClockid clockId, KernelTimespec* tp);
int APS5_VABI sceKernelClockGetres(KernelClockid clockId, KernelTimespec* tp);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
int APS5_VABI gettimeofday_nid_postfix(KernelTimeval* tv, KernelTimezone* tz);
std::int64_t APS5_VABI _Xtime_get_ticks_nid_postfix();
int APS5_VABI sceKernelConvertLocaltimeToUtc(std::int64_t, std::int64_t, std::int64_t*, KernelTimesec*, std::int32_t*);
int APS5_VABI sceKernelConvertUtcToLocaltime(std::int64_t, std::int64_t*, KernelTimesec*, std::uint64_t*);
}

static constexpr int SCE_OK = 0;

static constexpr int GUEST_CLOCK_REALTIME = 0;
static constexpr int GUEST_CLOCK_VIRTUAL = 1;
static constexpr int GUEST_CLOCK_PROF = 2;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_SECOND = 13;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t NANOS_PER_MILLISECOND = 1000000LL;
static constexpr std::int64_t BURN_NANOS = 200 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t IDLE_LIMIT_NANOS = 100 * NANOS_PER_MILLISECOND;
static constexpr std::int64_t GIVE_UP_NANOS = 10 * NANOS_PER_SECOND;

static void Require(bool value) { if (!value) std::abort(); }

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
    constexpr std::uint64_t sentinel = 0xa5a5a5a5a5a5a5a5ull;
    struct GuardedState {
        std::uint64_t before;
        KernelTimesec state;
        std::uint64_t after;
    };
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
        std::uint64_t universalDst = sentinel;
        Require(sceKernelConvertUtcToLocaltime(utc, &converted, &universal.state, &universalDst) == SCE_OK);
        Require(converted == seconds && universalDst == 0);
        Require(universal.state.t == seconds && universal.state.west_sec == 0 && universal.state.dst_sec == 0);
        Require(universal.before == sentinel && universal.after == sentinel);
    }
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, nullptr) == SCE_OK);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, nullptr) == SCE_OK);
}

int main() {
    CalendarConversionsInitializeTheirState();
    SecondClockReportsWholeSeconds();

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
