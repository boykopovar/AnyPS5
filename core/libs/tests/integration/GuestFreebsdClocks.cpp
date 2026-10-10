#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <string>

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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int sceOk = 0;

constexpr int guestClockRealtime = 0;
constexpr int guestClockVirtual = 1;
constexpr int guestClockProf = 2;
constexpr int guestClockMonotonic = 4;
constexpr int guestClockSecond = 13;
constexpr int guestClockProcessCputimeId = 15;

constexpr std::int64_t nanosPerSecond = 1000000000LL;
constexpr std::int64_t nanosPerMillisecond = 1000000LL;
constexpr std::int64_t burnNanos = 200 * nanosPerMillisecond;
constexpr std::int64_t idleLimitNanos = 100 * nanosPerMillisecond;
constexpr std::int64_t giveUpNanos = 10 * nanosPerSecond;
constexpr std::uint64_t sentinel = 0xa5a5a5a5a5a5a5a5ull;

std::string Clock(int clockId) {
    return "clock " + std::to_string(clockId);
}

void RequireValidTimespec(const KernelTimespec& time, const std::string& context) {
    Require(time.tv_sec >= 0, context + " seconds non-negative, got " + std::to_string(time.tv_sec));
    Require(time.tv_nsec >= 0 && time.tv_nsec < nanosPerSecond,
            context + " nanoseconds in [0, 1e9), got " + std::to_string(time.tv_nsec));
}

KernelTimespec Read(int clockId) {
    KernelTimespec time{-1, -1};
    RequireEqual(clock_gettime_nid_postfix(clockId, &time), sceOk, "clock_gettime " + Clock(clockId));
    RequireValidTimespec(time, "clock_gettime " + Clock(clockId));
    return time;
}

std::int64_t Nanos(int clockId) {
    const KernelTimespec time = Read(clockId);
    return time.tv_sec * nanosPerSecond + time.tv_nsec;
}

std::int64_t ResolutionNanos(int clockId) {
    KernelTimespec resolution{-1, -1};
    RequireEqual(clock_getres_nid_postfix(clockId, &resolution), sceOk, "clock_getres " + Clock(clockId));
    RequireEqual(resolution.tv_sec, std::int64_t{0}, "clock_getres seconds " + Clock(clockId));
    Require(resolution.tv_nsec > 0 && resolution.tv_nsec < nanosPerSecond,
            "clock_getres nanoseconds in (0, 1e9) for " + Clock(clockId) + ", got " + std::to_string(resolution.tv_nsec));
    KernelTimespec sceResolution{-1, -1};
    RequireEqual(sceKernelClockGetres(clockId, &sceResolution), sceOk, "sceKernelClockGetres " + Clock(clockId));
    RequireEqual(sceResolution.tv_sec, resolution.tv_sec, "sceKernelClockGetres seconds " + Clock(clockId));
    RequireEqual(sceResolution.tv_nsec, resolution.tv_nsec, "sceKernelClockGetres nanoseconds " + Clock(clockId));
    return resolution.tv_nsec;
}

void RequireNeverDecreases(int clockId) {
    std::int64_t previous = Nanos(clockId);
    for (int i = 0; i < 10000; ++i) {
        const std::int64_t current = Nanos(clockId);
        Require(current >= previous, Clock(clockId) + " went from " + std::to_string(previous) + " to " +
                std::to_string(current) + " at iteration " + std::to_string(i));
        previous = current;
    }
}

std::int64_t Microseconds() {
    KernelTimeval time{-1, -1};
    RequireEqual(gettimeofday_nid_postfix(&time, nullptr), sceOk, "gettimeofday");
    return time.tv_sec * 1000000LL + time.tv_usec;
}

struct GuardedState {
    std::uint64_t before;
    KernelTimesec state;
    std::uint64_t after;
};

GuardedState PoisonedState() {
    return {sentinel, {-1, 0xa5a5a5a5u, 0xa5a5a5a5u}, sentinel};
}

void RequireInitializedState(const GuardedState& guarded, std::int64_t seconds, const std::string& context) {
    RequireEqual(guarded.state.t, seconds, context + " state time");
    RequireEqual(guarded.state.west_sec, 0u, context + " state west_sec");
    RequireEqual(guarded.state.dst_sec, 0u, context + " state dst_sec");
    RequireEqual(guarded.before, sentinel, context + " guard before state");
    RequireEqual(guarded.after, sentinel, context + " guard after state");
}

constexpr std::array<std::int64_t, 4> calendarSeconds{-86400, 0, 1767225600, 2147483648};

const Case localToUtc{"ConvertLocaltimeToUtc_KnownSeconds_ReturnsSameSecondsAndInitializesState", [] {
    for (const std::int64_t seconds : calendarSeconds) {
        const std::string context = "local " + std::to_string(seconds);
        GuardedState local = PoisonedState();
        std::int64_t utc = -1;
        std::int32_t dst = -1;
        RequireEqual(sceKernelConvertLocaltimeToUtc(seconds, 0, &utc, &local.state, &dst), sceOk, context);
        RequireEqual(utc, seconds, context + " utc");
        RequireEqual(dst, 0, context + " dst");
        RequireInitializedState(local, seconds, context);
    }
}};

const Case utcToLocal{"ConvertUtcToLocaltime_KnownSeconds_ReturnsSameSecondsAndInitializesState", [] {
    for (const std::int64_t seconds : calendarSeconds) {
        const std::string context = "utc " + std::to_string(seconds);
        GuardedState local = PoisonedState();
        std::int64_t utc = -1;
        std::int32_t dst = -1;
        RequireEqual(sceKernelConvertLocaltimeToUtc(seconds, 0, &utc, &local.state, &dst), sceOk, context + " to utc");
        GuardedState universal = PoisonedState();
        std::int64_t converted = -1;
        std::uint64_t universalDst = sentinel;
        RequireEqual(sceKernelConvertUtcToLocaltime(utc, &converted, &universal.state, &universalDst), sceOk, context);
        RequireEqual(converted, seconds, context + " local");
        RequireEqual(universalDst, std::uint64_t{0}, context + " dst");
        RequireInitializedState(universal, seconds, context);
    }
}};

const Case nullOutputs{"ConvertLocaltimeAndUtc_NullOutputs_Succeed", [] {
    RequireEqual(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, nullptr), sceOk, "local to utc");
    RequireEqual(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, nullptr), sceOk, "utc to local");
}};

const Case secondResolution{"ClockGetres_SecondClock_ReportsOneSecond", [] {
    KernelTimespec resolution{-1, -1};
    RequireEqual(clock_getres_nid_postfix(guestClockSecond, &resolution), sceOk, "clock_getres");
    RequireEqual(resolution.tv_sec, std::int64_t{1}, "clock_getres seconds");
    RequireEqual(resolution.tv_nsec, std::int64_t{0}, "clock_getres nanoseconds");
    resolution = {-1, -1};
    RequireEqual(sceKernelClockGetres(guestClockSecond, &resolution), sceOk, "sceKernelClockGetres");
    RequireEqual(resolution.tv_sec, std::int64_t{1}, "sceKernelClockGetres seconds");
    RequireEqual(resolution.tv_nsec, std::int64_t{0}, "sceKernelClockGetres nanoseconds");
}};

const Case secondSceGettime{"SceKernelClockGettime_SecondClock_ReportsWholePositiveSeconds", [] {
    KernelTimespec second{-1, -1};
    RequireEqual(sceKernelClockGettime(guestClockSecond, &second), sceOk, "sceKernelClockGettime");
    Require(second.tv_sec > 0, "seconds positive, got " + std::to_string(second.tv_sec));
    RequireEqual(second.tv_nsec, std::int64_t{0}, "nanoseconds");
}};

const Case secondTracksRealtime{"ClockGettime_SecondClock_WholeSecondsWithinRealtimeAndNonDecreasing", [] {
    std::int64_t previous = 0;
    for (int i = 0; i < 1000; ++i) {
        const std::string context = "iteration " + std::to_string(i);
        const KernelTimespec before = Read(guestClockRealtime);
        const KernelTimespec second = Read(guestClockSecond);
        const KernelTimespec after = Read(guestClockRealtime);
        RequireEqual(second.tv_nsec, std::int64_t{0}, context + " nanoseconds");
        Require(second.tv_sec >= before.tv_sec - 1 && second.tv_sec <= after.tv_sec,
                context + " second " + std::to_string(second.tv_sec) + " within realtime [" +
                std::to_string(before.tv_sec) + " - 1, " + std::to_string(after.tv_sec) + "]");
        Require(second.tv_sec >= previous, context + " second " + std::to_string(second.tv_sec) +
                " not below previous " + std::to_string(previous));
        previous = second.tv_sec;
    }
}};

const Case sceGettimeProcessClocks{"SceKernelClockGettime_VirtualAndProfClocks_ReturnValidTimespec", [] {
    for (const int clockId : {guestClockVirtual, guestClockProf}) {
        KernelTimespec time{-1, -1};
        RequireEqual(sceKernelClockGettime(clockId, &time), sceOk, "sceKernelClockGettime " + Clock(clockId));
        RequireValidTimespec(time, "sceKernelClockGettime " + Clock(clockId));
    }
}};

const Case virtualMonotonic{"ClockGettime_VirtualClock_NeverDecreases", [] {
    RequireNeverDecreases(guestClockVirtual);
}};

const Case profMonotonic{"ClockGettime_ProfClock_NeverDecreases", [] {
    RequireNeverDecreases(guestClockProf);
}};

const Case processResolutions{"ClockGetres_VirtualAndProf_ReportSameSubsecondResolution", [] {
    const std::int64_t profResolution = ResolutionNanos(guestClockProf);
    RequireEqual(ResolutionNanos(guestClockVirtual), profResolution, "virtual resolution");
}};

const Case processOrdering{"ClockGettime_ProcessClocks_VirtualAtMostProfAtMostExecution", [] {
    const std::int64_t profResolution = ResolutionNanos(guestClockProf);
    for (int i = 0; i < 1000; ++i) {
        const std::int64_t user = Nanos(guestClockVirtual);
        const std::int64_t userAndSystem = Nanos(guestClockProf);
        const std::int64_t execution = Nanos(guestClockProcessCputimeId);
        const std::string context = "iteration " + std::to_string(i) + ": virtual " + std::to_string(user) +
                                    ", prof " + std::to_string(userAndSystem) + ", execution " + std::to_string(execution);
        Require(user <= userAndSystem, context + ", virtual at most prof");
        Require(userAndSystem <= execution + profResolution, context + ", prof at most execution plus resolution");
    }
}};

const Case busyProcess{"ClockGettime_BusyProcess_AccumulatesUserTimeAlsoCountedInProf", [] {
    const std::int64_t wallStart = Nanos(guestClockMonotonic);
    const std::int64_t profStart = Nanos(guestClockProf);
    const std::int64_t userStart = Nanos(guestClockVirtual);
    std::int64_t user = userStart;
    volatile std::uint64_t work = 0;
    while (user - userStart < burnNanos) {
        Require(Nanos(guestClockMonotonic) - wallStart < giveUpNanos,
                "user time reaches 200 ms within 10 s, got " + std::to_string(user - userStart) + " ns");
        for (int i = 0; i < 1000000; ++i) work = work + 1;
        const std::int64_t current = Nanos(guestClockVirtual);
        Require(current >= user, "virtual clock went from " + std::to_string(user) + " to " + std::to_string(current));
        user = current;
    }
    const std::int64_t profSpent = Nanos(guestClockProf) - profStart;
    Require(profSpent >= user - userStart, "prof time " + std::to_string(profSpent) + " ns at least user time " +
            std::to_string(user - userStart) + " ns");
}};

const Case idleProcess{"ClockGettime_IdleProcess_AccumulatesAtMost100MsCpuTime", [] {
    const std::int64_t userStart = Nanos(guestClockVirtual);
    const std::int64_t profStart = Nanos(guestClockProf);
    RequireEqual(sceKernelUsleep_nid_postfix(200000), sceOk, "sceKernelUsleep");
    const std::int64_t userSpent = Nanos(guestClockVirtual) - userStart;
    const std::int64_t profSpent = Nanos(guestClockProf) - profStart;
    Require(userSpent <= idleLimitNanos, "virtual time while idle at most 100 ms, got " + std::to_string(userSpent) + " ns");
    Require(profSpent <= idleLimitNanos, "prof time while idle at most 100 ms, got " + std::to_string(profSpent) + " ns");
}};

const Case xtimeTicks{"XtimeGetTicks_BetweenGettimeofdayCalls_ReturnsMicroseconds", [] {
    for (int i = 0; i < 100; ++i) {
        const std::int64_t before = Microseconds();
        const std::int64_t ticks = _Xtime_get_ticks_nid_postfix();
        const std::int64_t after = Microseconds();
        Require(before <= ticks && ticks <= after, "iteration " + std::to_string(i) + ": ticks " + std::to_string(ticks) +
                " within [" + std::to_string(before) + ", " + std::to_string(after) + "]");
    }
}};

} // namespace
