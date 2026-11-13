#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libkernel/System/include/TimerSignals.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;

std::int64_t Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int SigevNone = 0;
constexpr int SigevSignal = 1;
constexpr int SigevThreadId = 4;
constexpr int SignalMax = 128;
constexpr int Sigalrm = 14;
constexpr int TimerAbstime = 1;
constexpr int FirstTimer = 3;
constexpr int TimerMax = 32;
constexpr long long NanosecondsPerSecond = 1000000000;
constexpr long long TickNanoseconds = 1000000;

using Nanoseconds = __int128;

struct Ktimer {
    bool live = false;
    int clock = ClockRealtime;
    Nanoseconds value = 0;
    Nanoseconds interval = 0;
    int notify = SigevNone;
    int signo = 0;
    std::int64_t sigval = 0;
    std::int64_t thread = 0;
};

static_assert(TimerMax == TimerSignalSlots);
static_assert(sizeof(GuestSigevent) == 80);

std::mutex g_ktimerLock;
Ktimer g_ktimers[TimerMax];
TimedWait::Condition g_ktimerChanged;
bool g_ktimerThreadStarted = false;

Nanoseconds ToNanoseconds(const KernelTimespec& time) {
    return static_cast<Nanoseconds>(time.tv_sec) * NanosecondsPerSecond + time.tv_nsec;
}

KernelTimespec ToTimespec(Nanoseconds time) {
    return {static_cast<std::int64_t>(time / NanosecondsPerSecond),
            static_cast<std::int64_t>(time % NanosecondsPerSecond)};
}

Nanoseconds ClockNow(int clock) {
    KernelTimespec now{};
    clock_gettime_nid_postfix(clock, &now);

    return ToNanoseconds(now);
}

bool FixTimespec(KernelTimespec& time) {
    if (time.tv_sec < 0 || time.tv_nsec < 0 || time.tv_nsec >= NanosecondsPerSecond) return false;
    if (time.tv_sec == 0 && time.tv_nsec != 0 && time.tv_nsec < TickNanoseconds) time.tv_nsec = TickNanoseconds;
    return true;
}

Ktimer* FindTimer(int id) {
    if (id < FirstTimer || id >= TimerMax || !g_ktimers[id].live) return nullptr;
    return &g_ktimers[id];
}

void Expire(Ktimer& timer, Nanoseconds now) {
    if (timer.value == 0 || now < timer.value) return;
    if (timer.interval == 0) {
        timer.value = 0;
        return;
    }
    timer.value += ((now - timer.value) / timer.interval + 1) * timer.interval;
}

GuestItimerspec Remaining(Ktimer& timer) {
    const Nanoseconds now = ClockNow(timer.clock);
    if (timer.notify == SigevNone) Expire(timer, now);
    GuestItimerspec result{ToTimespec(timer.interval), {}};
    if (timer.value != 0) result.it_value = timer.value > now ? ToTimespec(timer.value - now) : KernelTimespec{0, 1};
    return result;
}

void Fire(int id, Ktimer& timer, Nanoseconds now) {
    std::int64_t missed = 0;
    if (timer.interval == 0) timer.value = 0;
    else {
        timer.value += timer.interval;
        if (now >= timer.value) {
            const Nanoseconds late = (now - timer.value) / timer.interval + 1;
            missed = static_cast<std::int64_t>(late);
            timer.value += late * timer.interval;
        }
    }
    if (!SendTimerSignal({timer.signo, timer.sigval, id, timer.thread, timer.notify == SigevThreadId}, missed)) {
        timer.value = 0;
        timer.interval = 0;
    }
}

void RunKtimers() {
    constexpr Nanoseconds LongestWait = 3600 * NanosecondsPerSecond;
    std::unique_lock lock(g_ktimerLock);
    for (;;) {
        std::optional<std::uint64_t> wake;
        for (int id = FirstTimer; id < TimerMax; ++id) {
            Ktimer& timer = g_ktimers[id];
            if (!timer.live || timer.notify == SigevNone || timer.value == 0) continue;
            const Nanoseconds now = ClockNow(timer.clock);
            if (now >= timer.value) Fire(id, timer, now);
            if (timer.value == 0) continue;
            const Nanoseconds left = std::clamp<Nanoseconds>(timer.value - ClockNow(timer.clock), 0, LongestWait);
            const std::uint64_t at = TimedWait::NowNanos() + static_cast<std::uint64_t>(left);
            wake = wake ? std::min(*wake, at) : at;
        }
        if (wake) g_ktimerChanged.WaitUntil(lock, *wake);
        else g_ktimerChanged.Wait(lock);
    }
}

void NotifyKtimers() {
    if (!g_ktimerThreadStarted) {
        Libc::StartHostThread(RunKtimers).detach();
        g_ktimerThreadStarted = true;
    }
    g_ktimerChanged.NotifyAll();
}

}

extern "C" {

int APS5_VABI ktimer_create_nid_postfix(int clock, const GuestSigevent* event, int* timerid) {
    GuestSigevent request{SigevSignal, Sigalrm, 0, {}};
    if (event != nullptr && !GuestCopyIn_nid_no_patch(&request, event, sizeof(request))) return static_cast<int>(Fail(GuestEfault));
    if (clock != ClockRealtime && clock != ClockMonotonic) return static_cast<int>(Fail(GuestEinval));
    if (event != nullptr) {
        if (request.notify != SigevNone && request.notify != SigevSignal && request.notify != SigevThreadId)
            return static_cast<int>(Fail(GuestEinval));
        if (request.notify != SigevNone && (request.signo <= 0 || request.signo > SignalMax))
            return static_cast<int>(Fail(GuestEinval));
    }
    std::lock_guard lock(g_ktimerLock);
    int id = FirstTimer;
    while (id < TimerMax && g_ktimers[id].live) ++id;
    if (id == TimerMax) return static_cast<int>(Fail(GuestEagain));
    if (timerid == nullptr) return static_cast<int>(Fail(GuestEfault));
    if (event == nullptr) request.value = id;
    const std::int64_t thread = request.notify == SigevThreadId ? static_cast<std::int32_t>(request.extra[0]) : 0;
    g_ktimers[id] = {true, clock, 0, 0, request.notify, request.signo, request.value, thread};
    *timerid = id;
    return 0;
}

int APS5_VABI ktimer_delete_nid_postfix(int id) {
    std::lock_guard lock(g_ktimerLock);
    Ktimer* timer = FindTimer(id);
    if (timer == nullptr) return static_cast<int>(Fail(GuestEinval));
    *timer = {};
    ForgetTimerSignal(id);
    return 0;
}

int APS5_VABI ktimer_settime_nid_postfix(int id, int flags, const GuestItimerspec* value, GuestItimerspec* oldValue) {
    GuestItimerspec next{};
    if (!GuestCopyIn_nid_no_patch(&next, value, sizeof(next))) return static_cast<int>(Fail(GuestEfault));
    GuestItimerspec previous{};
    const std::int64_t armer = scePthreadGetthreadid();
    {
        std::lock_guard lock(g_ktimerLock);
        Ktimer* timer = FindTimer(id);
        if (timer == nullptr) return static_cast<int>(Fail(GuestEinval));
        if (!FixTimespec(next.it_value)) return static_cast<int>(Fail(GuestEinval));
        const bool armed = next.it_value.tv_sec != 0 || next.it_value.tv_nsec != 0;
        if (armed && !FixTimespec(next.it_interval)) return static_cast<int>(Fail(GuestEinval));
        if (!armed) next.it_interval = {};
        if (oldValue != nullptr) previous = Remaining(*timer);
        timer->interval = ToNanoseconds(next.it_interval);
        timer->value = ToNanoseconds(next.it_value);
        if (armed && (flags & TimerAbstime) == 0) timer->value += ClockNow(timer->clock);
        if (timer->notify == SigevSignal) timer->thread = armer;
        if (timer->notify != SigevNone) NotifyKtimers();
    }
    if (oldValue != nullptr && !GuestCopyOut_nid_no_patch(oldValue, &previous, sizeof(previous)))
        return static_cast<int>(Fail(GuestEfault));
    return 0;
}

int APS5_VABI ktimer_gettime_nid_postfix(int id, GuestItimerspec* value) {
    std::lock_guard lock(g_ktimerLock);
    Ktimer* timer = FindTimer(id);
    if (timer == nullptr) return static_cast<int>(Fail(GuestEinval));
    if (value == nullptr) return static_cast<int>(Fail(GuestEfault));
    *value = Remaining(*timer);
    return 0;
}

int APS5_VABI ktimer_getoverrun_nid_postfix(int id) {
    std::lock_guard lock(g_ktimerLock);
    if (FindTimer(id) == nullptr) return static_cast<int>(Fail(GuestEinval));
    return TimerSignalOverrun(id);
}

}
