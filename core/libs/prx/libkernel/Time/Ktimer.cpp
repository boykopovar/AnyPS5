#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <mutex>

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
};

std::mutex g_ktimerLock;
Ktimer g_ktimers[TimerMax];

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
    Expire(timer, now);
    GuestItimerspec result{ToTimespec(timer.interval), {}};
    if (timer.value != 0) result.it_value = timer.value > now ? ToTimespec(timer.value - now) : KernelTimespec{0, 1};
    return result;
}

}

extern "C" {

int APS5_VABI ktimer_create_nid_postfix(int clock, const GuestSigevent* event, int* timerid) {
    if (clock != ClockRealtime && clock != ClockMonotonic) return static_cast<int>(Fail(GuestEinval));
    if (event != nullptr) {
        if (event->notify != SigevNone && event->notify != SigevSignal && event->notify != SigevThreadId)
            return static_cast<int>(Fail(GuestEinval));
        if (event->notify != SigevNone && (event->signo <= 0 || event->signo > SignalMax))
            return static_cast<int>(Fail(GuestEinval));
    }
    std::lock_guard lock(g_ktimerLock);
    int id = FirstTimer;
    while (id < TimerMax && g_ktimers[id].live) ++id;
    if (id == TimerMax) return static_cast<int>(Fail(GuestEagain));
    if (timerid == nullptr) return static_cast<int>(Fail(GuestEfault));
    if (event == nullptr || event->notify != SigevNone) NotImplemented_nid_no_patch("ktimer_create delivering a signal");
    g_ktimers[id] = {true, clock, 0, 0};
    *timerid = id;
    return 0;
}

int APS5_VABI ktimer_delete_nid_postfix(int id) {
    std::lock_guard lock(g_ktimerLock);
    Ktimer* timer = FindTimer(id);
    if (timer == nullptr) return static_cast<int>(Fail(GuestEinval));
    *timer = {};
    return 0;
}

int APS5_VABI ktimer_settime_nid_postfix(int id, int flags, const GuestItimerspec* value, GuestItimerspec* oldValue) {
    if (value == nullptr) return static_cast<int>(Fail(GuestEfault));
    std::lock_guard lock(g_ktimerLock);
    Ktimer* timer = FindTimer(id);
    if (timer == nullptr) return static_cast<int>(Fail(GuestEinval));
    GuestItimerspec next = *value;
    if (!FixTimespec(next.it_value)) return static_cast<int>(Fail(GuestEinval));
    const bool armed = next.it_value.tv_sec != 0 || next.it_value.tv_nsec != 0;
    if (armed && !FixTimespec(next.it_interval)) return static_cast<int>(Fail(GuestEinval));
    if (!armed) next.it_interval = {};
    if (oldValue != nullptr) *oldValue = Remaining(*timer);
    timer->interval = ToNanoseconds(next.it_interval);
    timer->value = ToNanoseconds(next.it_value);
    if (armed && (flags & TimerAbstime) == 0) timer->value += ClockNow(timer->clock);
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
    return 0;
}

}
