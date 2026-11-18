#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;

constexpr int SigevNone = 0;
constexpr int SigevSignal = 1;
constexpr int SigevThreadId = 4;

constexpr int FirstTimer = 3;
constexpr int TimerMax = 32;

constexpr std::int64_t MaxItimerSeconds = 0x7fffffff / 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

void SetError(int err) {
    *__error_nid_postfix() = err;
}

void TestKtimerCreateBounds() {
    int id = -1;
    const GuestSigevent none{SigevNone, 0, 0, {}};

    const int invalidClocks[] = {-100, -1, 1, 2, 3, 5, 6, 10, 100};
    for (const auto clock : invalidClocks) {
        SetError(0);
        Require(ktimer_create_nid_postfix(clock, &none, &id) == -1);
        Require(Error() == PosixEinval);
        Require(id == -1);
    }

    const int invalidNotify[] = {-1, 2, 3, 5, 6, 10};
    for (const auto notify : invalidNotify) {
        GuestSigevent badEvent{notify, 0, 0, {}};
        SetError(0);
        Require(ktimer_create_nid_postfix(ClockMonotonic, &badEvent, &id) == -1);
        Require(Error() == PosixEinval);
    }

    const int invalidSignals[] = {-10, -1, 0, 129, 130, 200};
    for (const auto sig : invalidSignals) {
        GuestSigevent badSignal{SigevSignal, sig, 0, {}};
        SetError(0);
        Require(ktimer_create_nid_postfix(ClockMonotonic, &badSignal, &id) == -1);
        Require(Error() == PosixEinval);

        GuestSigevent badThreadSignal{SigevThreadId, sig, 0, {}};
        SetError(0);
        Require(ktimer_create_nid_postfix(ClockMonotonic, &badThreadSignal, &id) == -1);
        Require(Error() == PosixEinval);
    }

    SetError(0);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, reinterpret_cast<int*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    std::vector<int> createdIds;
    for (int expected = FirstTimer; expected < TimerMax; ++expected) {
        int newId = -1;
        const int clk = (expected % 2 == 0) ? ClockRealtime : ClockMonotonic;
        Require(ktimer_create_nid_postfix(clk, &none, &newId) == 0);
        Require(newId == expected);
        createdIds.push_back(newId);
    }

    SetError(0);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, &id) == -1);
    Require(Error() == PosixEagain);

    Require(ktimer_delete_nid_postfix(7) == 0);
    int reusedId = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, &reusedId) == 0);
    Require(reusedId == 7);

    for (const auto each : createdIds) {
        Require(ktimer_delete_nid_postfix(each) == 0);
    }
}

void TestKtimerDeleteBounds() {
    const GuestSigevent none{SigevNone, 0, 0, {}};
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, &id) == 0);
    Require(id >= FirstTimer && id < TimerMax);

    const int invalidIds[] = {-100, -1, 0, 1, 2, TimerMax, TimerMax + 1, 100, 9999};
    for (const auto badId : invalidIds) {
        SetError(0);
        Require(ktimer_delete_nid_postfix(badId) == -1);
        Require(Error() == PosixEinval);
    }

    Require(ktimer_delete_nid_postfix(id) == 0);
    SetError(0);
    Require(ktimer_delete_nid_postfix(id) == -1);
    Require(Error() == PosixEinval);
}

void TestKtimerSettimeAndGettimeBounds() {
    const GuestSigevent none{SigevNone, 0, 0, {}};
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, &id) == 0);

    SetError(0);
    Require(ktimer_settime_nid_postfix(-1, 0, nullptr, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(ktimer_settime_nid_postfix(id, 0, nullptr, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(ktimer_settime_nid_postfix(id, 0, reinterpret_cast<const GuestItimerspec*>(0x10), nullptr) == -1);
    Require(Error() == PosixEfault);

    GuestItimerspec spec{{0, 0}, {0, 0}};
    const int invalidIds[] = {-100, -1, 0, 1, 2, TimerMax, 100};
    for (const auto badId : invalidIds) {
        SetError(0);
        Require(ktimer_settime_nid_postfix(badId, 0, &spec, nullptr) == -1);
        Require(Error() == PosixEinval);

        SetError(0);
        Require(ktimer_gettime_nid_postfix(badId, &spec) == -1);
        Require(Error() == PosixEinval);

        SetError(0);
        Require(ktimer_gettime_nid_postfix(badId, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    SetError(0);
    Require(ktimer_gettime_nid_postfix(id, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(ktimer_gettime_nid_postfix(id, reinterpret_cast<GuestItimerspec*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    const GuestItimerspec invalidValues[] = {
        {{0, 0}, {-1, 0}},
        {{0, 0}, {0, -1}},
        {{0, 0}, {0, 1000000000}},
        {{0, 0}, {0, 1500000000}}
    };
    for (const auto& invalid : invalidValues) {
        SetError(0);
        Require(ktimer_settime_nid_postfix(id, 0, &invalid, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    const GuestItimerspec invalidIntervals[] = {
        {{-1, 0}, {1, 0}},
        {{0, -1}, {1, 0}},
        {{0, 1000000000}, {1, 0}},
        {{0, 2000000000}, {1, 0}}
    };
    for (const auto& invalid : invalidIntervals) {
        SetError(0);
        Require(ktimer_settime_nid_postfix(id, 0, &invalid, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    GuestItimerspec disarmWithBadInterval{{-1, -1}, {0, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &disarmWithBadInterval, nullptr) == 0);

    GuestItimerspec arm{{1, 0}, {5, 0}};
    SetError(0);
    Require(ktimer_settime_nid_postfix(id, 0, &arm, reinterpret_cast<GuestItimerspec*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    GuestItimerspec oldVal{{-1, -1}, {-1, -1}};
    Require(ktimer_settime_nid_postfix(id, 0, &arm, &oldVal) == 0);

    GuestItimerspec readBack{};
    Require(ktimer_gettime_nid_postfix(id, &readBack) == 0);
    Require(readBack.it_value.tv_sec >= 0 && readBack.it_value.tv_sec <= 5);
    Require(readBack.it_interval.tv_sec == 1 && readBack.it_interval.tv_nsec == 0);

    GuestItimerspec disarm{{0, 0}, {0, 0}};
    GuestItimerspec oldArmVal{{-1, -1}, {-1, -1}};
    Require(ktimer_settime_nid_postfix(id, 0, &disarm, &oldArmVal) == 0);
    Require(oldArmVal.it_value.tv_sec >= 0);

    Require(ktimer_gettime_nid_postfix(id, &readBack) == 0);
    Require(readBack.it_value.tv_sec == 0 && readBack.it_value.tv_nsec == 0);
    Require(readBack.it_interval.tv_sec == 0 && readBack.it_interval.tv_nsec == 0);

    Require(ktimer_delete_nid_postfix(id) == 0);
}

void TestKtimerGetoverrunBounds() {
    const GuestSigevent none{SigevNone, 0, 0, {}};
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &none, &id) == 0);

    Require(ktimer_getoverrun_nid_postfix(id) == 0);

    const int invalidIds[] = {-100, -1, 0, 1, 2, TimerMax, 100};
    for (const auto badId : invalidIds) {
        SetError(0);
        Require(ktimer_getoverrun_nid_postfix(badId) == -1);
        Require(Error() == PosixEinval);
    }

    Require(ktimer_delete_nid_postfix(id) == 0);
    SetError(0);
    Require(ktimer_getoverrun_nid_postfix(id) == -1);
    Require(Error() == PosixEinval);
}

void TestGetitimerBounds() {
    GuestItimerval val{{1, 2}, {3, 4}};
    const int invalidWhich[] = {-100, -1, 3, 4, 10, 100};
    for (const auto which : invalidWhich) {
        SetError(0);
        Require(getitimer_nid_postfix(which, &val) == -1);
        Require(Error() == PosixEinval);

        SetError(0);
        Require(getitimer_nid_postfix(which, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    for (int which = 0; which <= 2; ++which) {
        SetError(0);
        Require(getitimer_nid_postfix(which, nullptr) == -1);
        Require(Error() == PosixEfault);

        SetError(0);
        Require(getitimer_nid_postfix(which, reinterpret_cast<GuestItimerval*>(0x10)) == -1);
        Require(Error() == PosixEfault);
    }

    for (int cpuTimer : {1, 2}) {
        GuestItimerval cpuVal{{10, 20}, {30, 40}};
        Require(getitimer_nid_postfix(cpuTimer, &cpuVal) == 0);
        Require(cpuVal.value.tv_sec == 0 && cpuVal.value.tv_usec == 0);
        Require(cpuVal.interval.tv_sec == 0 && cpuVal.interval.tv_usec == 0);
    }

    GuestItimerval realVal{{10, 20}, {30, 40}};
    Require(getitimer_nid_postfix(0, &realVal) == 0);
    Require(realVal.value.tv_sec >= 0 && realVal.value.tv_usec >= 0 && realVal.value.tv_usec < 1000000);
    Require(realVal.interval.tv_sec >= 0 && realVal.interval.tv_usec >= 0 && realVal.interval.tv_usec < 1000000);
}

void TestSetitimerBounds() {
    SetError(0);
    Require(setitimer_nid_postfix(0, nullptr, nullptr) == -1);
    Require(Error() == PosixEfault);

    SetError(0);
    Require(setitimer_nid_postfix(0, reinterpret_cast<const GuestItimerval*>(0x10), nullptr) == -1);
    Require(Error() == PosixEfault);

    GuestItimerval zeroVal{{0, 0}, {0, 0}};
    const int invalidWhich[] = {-100, -1, 3, 4, 10, 100};
    for (const auto which : invalidWhich) {
        SetError(0);
        Require(setitimer_nid_postfix(which, &zeroVal, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    const GuestItimerval invalidValueTimes[] = {
        {{0, 0}, {-1, 0}},
        {{0, 0}, {MaxItimerSeconds + 1, 0}},
        {{0, 0}, {0, -1}},
        {{0, 0}, {0, 1000000}},
        {{0, 0}, {0, 2000000}}
    };
    for (const auto& invalid : invalidValueTimes) {
        SetError(0);
        Require(setitimer_nid_postfix(0, &invalid, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    const GuestItimerval invalidIntervalTimes[] = {
        {{-1, 0}, {1, 0}},
        {{MaxItimerSeconds + 1, 0}, {1, 0}},
        {{0, -1}, {1, 0}},
        {{0, 1000000}, {1, 0}},
        {{0, 3000000}, {1, 0}}
    };
    for (const auto& invalid : invalidIntervalTimes) {
        SetError(0);
        Require(setitimer_nid_postfix(0, &invalid, nullptr) == -1);
        Require(Error() == PosixEinval);
    }

    GuestItimerval prev{{-1, -1}, {-1, -1}};
    Require(setitimer_nid_postfix(0, &zeroVal, &prev) == 0);

    SetError(0);
    Require(setitimer_nid_postfix(0, nullptr, reinterpret_cast<GuestItimerval*>(0x10)) == -1);
    Require(Error() == PosixEfault);

    bool caughtVirtual = false;
    try {
        GuestItimerval armVal{{0, 0}, {1, 0}};
        setitimer_nid_postfix(1, &armVal, nullptr);
    } catch (const std::runtime_error&) {
        caughtVirtual = true;
    }
    Require(caughtVirtual);

    bool caughtProf = false;
    try {
        GuestItimerval armVal{{0, 0}, {1, 0}};
        setitimer_nid_postfix(2, &armVal, nullptr);
    } catch (const std::runtime_error&) {
        caughtProf = true;
    }
    Require(caughtProf);

    for (int cpuTimer : {1, 2}) {
        GuestItimerval cpuPrev{{-1, -1}, {-1, -1}};
        Require(setitimer_nid_postfix(cpuTimer, &zeroVal, &cpuPrev) == 0);
        Require(cpuPrev.value.tv_sec == 0 && cpuPrev.value.tv_usec == 0);
        Require(cpuPrev.interval.tv_sec == 0 && cpuPrev.interval.tv_usec == 0);

        SetError(0);
        Require(setitimer_nid_postfix(cpuTimer, nullptr, reinterpret_cast<GuestItimerval*>(0x10)) == -1);
        Require(Error() == PosixEfault);
    }
}

} // namespace

int main() {
    TestKtimerCreateBounds();
    TestKtimerDeleteBounds();
    TestKtimerSettimeAndGettimeBounds();
    TestKtimerGetoverrunBounds();
    TestGetitimerBounds();
    TestSetitimerBounds();
    return 0;
}
