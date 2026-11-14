#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

namespace {

constexpr int GuestClockMonotonic = 4;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc time check failed at line %u\n", location.line());
    std::abort();
}

std::int64_t MonotonicNanoseconds() {
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(GuestClockMonotonic, &now) == 0);
    return now.tv_sec * 1000000000 + now.tv_nsec;
}

void TestBracketedByMonotonicClock() {
    for (int round = 0; round < 100; ++round) {
        const std::int64_t before = MonotonicNanoseconds();
        const std::int64_t millis = _ZN3rtc10TimeMillisEv_nid_postfix();
        const std::int64_t micros = _ZN3rtc10TimeMicrosEv_nid_postfix();
        const std::int64_t after = MonotonicNanoseconds();
        Require(before / 1000000 <= millis && millis <= after / 1000000);
        Require(before / 1000 <= micros && micros <= after / 1000);
    }
}

void TestNeverDecreases() {
    std::int64_t lastMillis = _ZN3rtc10TimeMillisEv_nid_postfix();
    std::int64_t lastMicros = _ZN3rtc10TimeMicrosEv_nid_postfix();
    for (int round = 0; round < 10000; ++round) {
        const std::int64_t millis = _ZN3rtc10TimeMillisEv_nid_postfix();
        const std::int64_t micros = _ZN3rtc10TimeMicrosEv_nid_postfix();
        Require(millis >= lastMillis && micros >= lastMicros);
        lastMillis = millis;
        lastMicros = micros;
    }
}

void TestAdvancesWithSleep() {
    const std::int64_t millis = _ZN3rtc10TimeMillisEv_nid_postfix();
    const std::int64_t micros = _ZN3rtc10TimeMicrosEv_nid_postfix();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(_ZN3rtc10TimeMillisEv_nid_postfix() - millis >= 19);
    Require(_ZN3rtc10TimeMicrosEv_nid_postfix() - micros >= 19000);
}

}

int main() {
    TestBracketedByMonotonicClock();
    TestNeverDecreases();
    TestAdvancesWithSleep();
    std::puts("libwebrtc time tests passed");
}
