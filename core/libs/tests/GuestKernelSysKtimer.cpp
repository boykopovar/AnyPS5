#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
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

const GuestSigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::int64_t Nanoseconds(const KernelTimespec& time) {
    return time.tv_sec * Second + time.tv_nsec;
}

int Create(int clock = ClockMonotonic) {
    int id = -1;
    Require(ktimer_create_nid_postfix(clock, &None, &id) == 0);
    return id;
}

GuestItimerspec Get(int id) {
    GuestItimerspec value{{-1, -1}, {-1, -1}};
    Require(ktimer_gettime_nid_postfix(id, &value) == 0);
    return value;
}

void Sleep(std::int64_t nanoseconds) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

void Creation() {
    int id = -1;
    const GuestSigevent badNotify{2, 0, 0, {}};
    const GuestSigevent noSignal{1, 0, 0, {}};
    const GuestSigevent badSignal{4, 129, 0, {}};
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
    GuestItimerspec value{};
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
    GuestItimerspec disarmed = Get(id);
    Require(Nanoseconds(disarmed.it_value) == 0 && Nanoseconds(disarmed.it_interval) == 0);
    for (const GuestItimerspec bad : {GuestItimerspec{{0, 0}, {-1, 0}}, GuestItimerspec{{0, 0}, {0, -1}}, GuestItimerspec{{0, 0}, {0, Second}},
                                 GuestItimerspec{{-1, 0}, {1, 0}}, GuestItimerspec{{0, Second}, {1, 0}}})
        Require(ktimer_settime_nid_postfix(id, 0, &bad, nullptr) == -1 && Error() == Einval);
    const GuestItimerspec valid{{0, 0}, {1, 0}};
    for (const int badFlags : {-1, 2, ~TimerAbstime})
        Require(ktimer_settime_nid_postfix(id, badFlags, &valid, nullptr) == -1 && Error() == Einval);

    const GuestItimerspec clear{{-1, 0}, {0, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &clear, nullptr) == 0);
    disarmed = Get(id);
    Require(Nanoseconds(disarmed.it_value) == 0 && Nanoseconds(disarmed.it_interval) == 0);

    const GuestItimerspec small{{0, 1}, {10, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &small, nullptr) == 0);
    GuestItimerspec old{};
    const GuestItimerspec later{{0, 0}, {20, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &later, &old) == 0);
    Require(Nanoseconds(old.it_interval) == Millisecond);
    Require(Nanoseconds(old.it_value) > 9 * Second && Nanoseconds(old.it_value) <= 10 * Second);
    const GuestItimerspec tiny{{0, 0}, {0, 1}};
    Require(ktimer_settime_nid_postfix(id, 0, &tiny, &old) == 0);
    Require(Nanoseconds(old.it_value) > 19 * Second && Nanoseconds(old.it_value) <= 20 * Second);
    const GuestItimerspec off{};
    Require(ktimer_settime_nid_postfix(id, 0, &off, &old) == 0);
    Require(Nanoseconds(old.it_interval) == 0 && Nanoseconds(old.it_value) <= Millisecond);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Expiry() {
    const int id = Create();
    const GuestItimerspec once{{0, 0}, {0, 30 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    const auto running = Get(id);
    Require(Nanoseconds(running.it_value) > 0 && Nanoseconds(running.it_value) <= 30 * Millisecond);
    Sleep(60 * Millisecond);
    const auto expired = Get(id);
    Require(Nanoseconds(expired.it_value) == 0 && Nanoseconds(expired.it_interval) == 0);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);

    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    Sleep(55 * Millisecond);
    const auto period = Get(id);
    Require(Nanoseconds(period.it_interval) == 10 * Millisecond);
    Require(Nanoseconds(period.it_value) > 0 && Nanoseconds(period.it_value) <= 10 * Millisecond);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Absolute() {
    for (const int clock : {ClockRealtime, ClockMonotonic}) {
        const int id = Create(clock);
        KernelTimespec now{};
        Require(clock_gettime_nid_postfix(clock, &now) == 0);
        const GuestItimerspec future{{0, 0}, {now.tv_sec + 5, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &future, nullptr) == 0);
        const auto remaining = Get(id);
        Require(Nanoseconds(remaining.it_value) > 4 * Second && Nanoseconds(remaining.it_value) <= 5 * Second);

        const GuestItimerspec past{{0, 0}, {now.tv_sec - 1, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &past, nullptr) == 0);
        Require(Nanoseconds(Get(id).it_value) == 0);
        const GuestItimerspec pastPeriodic{{1, 0}, {now.tv_sec - 10, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &pastPeriodic, nullptr) == 0);
        const auto moved = Get(id);
        Require(Nanoseconds(moved.it_value) > 0 && Nanoseconds(moved.it_value) <= Second);
        Require(Nanoseconds(moved.it_interval) == Second);
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
