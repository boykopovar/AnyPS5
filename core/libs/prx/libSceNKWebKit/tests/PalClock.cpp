#include "prx/libSceNKWebKit/include/PalClockExports.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>

extern "C" void CurrentTimeClockForTest_nid_no_patch(int (*query)(int, KernelTimespec*));

namespace {

using NKWebKit::PalAbi::Clock;

double currentTime = 1000.25;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PAL clock check failed at line %u\n", location.line());
        std::abort();
    }
}

int QueryClock(int clockId, KernelTimespec* result) {
    Require(clockId == 4);
    const auto seconds = static_cast<std::int64_t>(currentTime);
    *result = KernelTimespec{seconds, static_cast<std::int64_t>((currentTime - seconds) * 1'000'000'000.0)};
    return 0;
}

Clock* Create() {
    Clock* clock = nullptr;
    Require(_ZN3PAL5Clock6createEv(&clock) == &clock);
    Require(clock && clock->vptr);
    return clock;
}

template<typename Function>
void RequireRejected(Function function) {
    const int previousErrno = errno;
    errno = 37;
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
    Require(errno == 37);
    errno = previousErrno;
}

void TestPlayback() {
    auto* clock = Create();
    const auto& calls = *clock->vptr;
    Require(!calls.isRunning(clock));
    Require(calls.playRate(clock) == 1.0);
    Require(calls.currentTime(clock) == 0.0);

    currentTime = 1005.25;
    Require(calls.currentTime(clock) == 0.0);
    calls.setCurrentTime(clock, -3.5);
    calls.setPlayRate(clock, 2.0);
    Require(calls.currentTime(clock) == -3.5);
    calls.start(clock);
    Require(calls.isRunning(clock));
    currentTime = 1006.75;
    Require(calls.currentTime(clock) == -0.5);
    calls.start(clock);
    currentTime = 1008.25;
    Require(calls.currentTime(clock) == 2.5);

    calls.setPlayRate(clock, -0.5);
    Require(calls.playRate(clock) == -0.5);
    Require(calls.currentTime(clock) == 2.5);
    currentTime = 1012.25;
    Require(calls.currentTime(clock) == 0.5);
    calls.stop(clock);
    Require(!calls.isRunning(clock));
    currentTime = 2000.0;
    Require(calls.currentTime(clock) == 0.5);
    calls.stop(clock);
    Require(calls.currentTime(clock) == 0.5);

    calls.start(clock);
    currentTime = 2002.0;
    Require(calls.currentTime(clock) == -0.5);
    calls.setCurrentTime(clock, 99.0);
    Require(calls.isRunning(clock));
    currentTime = 2005.0;
    Require(calls.currentTime(clock) == 97.5);
    calls.setPlayRate(clock, 0.0);
    currentTime = 3000.0;
    Require(calls.currentTime(clock) == 97.5);
    calls.setPlayRate(clock, -2.0);
    currentTime = 3000.5;
    Require(calls.currentTime(clock) == 96.5);
    calls.stop(clock);

    auto* independent = Create();
    Require(independent != clock);
    Require(!independent->vptr->isRunning(independent));
    Require(independent->vptr->currentTime(independent) == 0.0);
    Require(calls.currentTime(clock) == 96.5);
    independent->vptr->deletingDestructor(independent);

    const auto* originalVtable = clock->vptr;
    calls.completeDestructor(clock);
    Require(clock->vptr == originalVtable);
    calls.deletingDestructor(clock);
}

void TestSpecialValues() {
    auto* clock = Create();
    const auto& calls = *clock->vptr;
    for (double value : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        calls.setPlayRate(clock, 1.0);
        calls.setCurrentTime(clock, value);
        const double time = calls.currentTime(clock);
        Require(std::isnan(value) ? std::isnan(time) : time == value);
        calls.setCurrentTime(clock, 4.0);
        calls.setPlayRate(clock, value);
        const double rate = calls.playRate(clock);
        Require(std::isnan(value) ? std::isnan(rate) : rate == value);
        Require(std::isnan(calls.currentTime(clock)));
    }
    calls.deletingDestructor(clock);
}

void TestInvalidStorage() {
    RequireRejected([] { _ZN3PAL5Clock6createEv(nullptr); });
    RequireRejected([] { _ZN3PAL5Clock6createEv(reinterpret_cast<Clock**>(std::uintptr_t{0x10})); });
    auto* clock = Create();
    const auto& calls = *clock->vptr;
    for (Clock* invalid : {static_cast<Clock*>(nullptr), reinterpret_cast<Clock*>(std::uintptr_t{0x10})}) {
        RequireRejected([&] { calls.setCurrentTime(invalid, 1.0); });
        RequireRejected([&] { calls.currentTime(invalid); });
        RequireRejected([&] { calls.setPlayRate(invalid, 2.0); });
        RequireRejected([&] { calls.playRate(invalid); });
        RequireRejected([&] { calls.start(invalid); });
        RequireRejected([&] { calls.stop(invalid); });
        RequireRejected([&] { calls.isRunning(invalid); });
    }
    calls.deletingDestructor(clock);
}

}  // namespace

int main() {
    CurrentTimeClockForTest_nid_no_patch(QueryClock);
    TestPlayback();
    TestSpecialValues();
    TestInvalidStorage();
    CurrentTimeClockForTest_nid_no_patch(nullptr);
    std::puts("PAL clock: playback, pause/resume, seeking, rates, special values, independent clocks and checked storage passed");
}
