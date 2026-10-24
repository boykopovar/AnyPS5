#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>
#include <vector>

namespace {

struct Sigevent {
    int notify;
    int signo;
    std::int64_t value;
    std::int64_t extra[6];
};

struct Timespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

struct Itimerspec {
    Timespec interval;
    Timespec value;
};

}

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI clock_gettime_nid_postfix(int clock, Timespec* time);
int APS5_VABI ktimer_create_nid_postfix(int clock, const Sigevent* event, int* timerid);
int APS5_VABI ktimer_delete_nid_postfix(int timerid);
int APS5_VABI ktimer_settime_nid_postfix(int timerid, int flags, const Itimerspec* value, Itimerspec* oldValue);
int APS5_VABI ktimer_gettime_nid_postfix(int timerid, Itimerspec* value);
int APS5_VABI ktimer_getoverrun_nid_postfix(int timerid);
}

namespace {

constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;
constexpr int TimerAbstime = 1;
constexpr std::int64_t Millisecond = 1000000;
constexpr std::int64_t Second = 1000000000;

const Sigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::int64_t Nanoseconds(const Timespec& time) {
    return time.seconds * Second + time.nanoseconds;
}

int Create(int clock = ClockMonotonic) {
    int id = -1;
    Require(ktimer_create_nid_postfix(clock, &None, &id) == 0);
    return id;
}

Itimerspec Get(int id) {
    Itimerspec value{{-1, -1}, {-1, -1}};
    Require(ktimer_gettime_nid_postfix(id, &value) == 0);
    return value;
}

void Sleep(std::int64_t nanoseconds) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

void Creation() {
    int id = -1;
    const Sigevent badNotify{2, 0, 0, {}};
    const Sigevent noSignal{1, 0, 0, {}};
    const Sigevent badSignal{4, 129, 0, {}};
    Require(ktimer_create_nid_postfix(ClockUptime, &None, &id) == -1 && Error() == Einval && id == -1);
    Require(ktimer_create_nid_postfix(-1, &None, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badNotify, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &noSignal, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badSignal, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, nullptr) == -1 && Error() == Efault);

    std::vector<int> ids;
    for (int expected = 3; expected < 32; ++expected) {
        ids.push_back(Create(expected % 2 == 0 ? ClockRealtime : ClockMonotonic));
        Require(ids.back() == expected);
    }
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, &id) == -1 && Error() == Eagain);
    Require(ktimer_delete_nid_postfix(7) == 0);
    Require(Create() == 7);
    for (const int each : ids) Require(ktimer_delete_nid_postfix(each) == 0);
}

void InvalidIds() {
    Itimerspec value{};
    const int id = Create();
    Require(ktimer_delete_nid_postfix(id) == 0);
    for (const int bad : {-1, 0, 2, id, 32}) {
        Require(ktimer_delete_nid_postfix(bad) == -1 && Error() == Einval);
        Require(ktimer_settime_nid_postfix(bad, 0, &value, nullptr) == -1 && Error() == Einval);
        Require(ktimer_gettime_nid_postfix(bad, &value) == -1 && Error() == Einval);
        Require(ktimer_getoverrun_nid_postfix(bad) == -1 && Error() == Einval);
    }
    Require(ktimer_settime_nid_postfix(id, 0, nullptr, nullptr) == -1 && Error() == Efault);
}

void Values() {
    const int id = Create();
    Itimerspec disarmed = Get(id);
    Require(Nanoseconds(disarmed.value) == 0 && Nanoseconds(disarmed.interval) == 0);
    for (const Itimerspec bad : {Itimerspec{{0, 0}, {-1, 0}}, Itimerspec{{0, 0}, {0, -1}}, Itimerspec{{0, 0}, {0, Second}},
                                 Itimerspec{{-1, 0}, {1, 0}}, Itimerspec{{0, Second}, {1, 0}}})
        Require(ktimer_settime_nid_postfix(id, 0, &bad, nullptr) == -1 && Error() == Einval);

    const Itimerspec clear{{-1, 0}, {0, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &clear, nullptr) == 0);
    disarmed = Get(id);
    Require(Nanoseconds(disarmed.value) == 0 && Nanoseconds(disarmed.interval) == 0);

    const Itimerspec small{{0, 1}, {10, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &small, nullptr) == 0);
    Itimerspec old{};
    const Itimerspec later{{0, 0}, {20, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &later, &old) == 0);
    Require(Nanoseconds(old.interval) == Millisecond);
    Require(Nanoseconds(old.value) > 9 * Second && Nanoseconds(old.value) <= 10 * Second);
    const Itimerspec tiny{{0, 0}, {0, 1}};
    Require(ktimer_settime_nid_postfix(id, 0, &tiny, &old) == 0);
    Require(Nanoseconds(old.value) > 19 * Second && Nanoseconds(old.value) <= 20 * Second);
    const Itimerspec off{};
    Require(ktimer_settime_nid_postfix(id, 0, &off, &old) == 0);
    Require(Nanoseconds(old.interval) == 0 && Nanoseconds(old.value) <= Millisecond);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Expiry() {
    const int id = Create();
    const Itimerspec once{{0, 0}, {0, 30 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    const auto running = Get(id);
    Require(Nanoseconds(running.value) > 0 && Nanoseconds(running.value) <= 30 * Millisecond);
    Sleep(60 * Millisecond);
    const auto expired = Get(id);
    Require(Nanoseconds(expired.value) == 0 && Nanoseconds(expired.interval) == 0);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);

    const Itimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    Sleep(55 * Millisecond);
    const auto period = Get(id);
    Require(Nanoseconds(period.interval) == 10 * Millisecond);
    Require(Nanoseconds(period.value) > 0 && Nanoseconds(period.value) <= 10 * Millisecond);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Absolute() {
    for (const int clock : {ClockRealtime, ClockMonotonic}) {
        const int id = Create(clock);
        Timespec now{};
        Require(clock_gettime_nid_postfix(clock, &now) == 0);
        const Itimerspec future{{0, 0}, {now.seconds + 5, now.nanoseconds}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &future, nullptr) == 0);
        const auto remaining = Get(id);
        Require(Nanoseconds(remaining.value) > 4 * Second && Nanoseconds(remaining.value) <= 5 * Second);

        const Itimerspec past{{0, 0}, {now.seconds - 1, now.nanoseconds}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &past, nullptr) == 0);
        Require(Nanoseconds(Get(id).value) == 0);
        const Itimerspec pastPeriodic{{1, 0}, {now.seconds - 10, now.nanoseconds}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &pastPeriodic, nullptr) == 0);
        const auto moved = Get(id);
        Require(Nanoseconds(moved.value) > 0 && Nanoseconds(moved.value) <= Second);
        Require(Nanoseconds(moved.interval) == Second);
        Require(ktimer_delete_nid_postfix(id) == 0);
    }
}

}

int main() {
    Creation();
    InvalidIds();
    Values();
    Expiry();
    Absolute();
    std::puts("guest_kernel_sys_ktimer passed");
    return 0;
}
