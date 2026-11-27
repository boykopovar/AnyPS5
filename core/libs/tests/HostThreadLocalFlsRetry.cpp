#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/PreciseWait.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace {

struct Counter {
    int value = 0;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::fflush(stderr);
    std::exit(1);
}

template<typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    std::vector<DWORD> slots;
    for (DWORD slot = FlsAlloc(nullptr); slot != FLS_OUT_OF_INDEXES; slot = FlsAlloc(nullptr)) slots.push_back(slot);
    Require(!slots.empty(), "no fiber-local slot was free before the test");

    const bool valueThrew = Throws([] { HostThreadLocal<Counter, Counter>(); });
    HANDLE timer = INVALID_HANDLE_VALUE;
    const bool timerThrew = Throws([&] { timer = PreciseWaitThreadTimer(); });
    const auto sleepStart = std::chrono::steady_clock::now();
    const bool sleepThrew = Throws([] { PreciseSleepUs(2000); });
    const auto slept = std::chrono::steady_clock::now() - sleepStart;
    for (const DWORD slot : slots) FlsFree(slot);

    Require(valueThrew, "HostThreadLocal returned a value without a fiber-local slot");
    Require(!timerThrew, "the precise wait timer threw without a fiber-local slot");
    Require(timer == nullptr, "the precise wait timer had a handle without a fiber-local slot");
    Require(!sleepThrew, "a precise sleep threw without a fiber-local slot");
    Require(slept >= std::chrono::milliseconds(2), "a precise sleep without a fiber-local slot returned early");

    bool retryThrew = Throws([] { HostThreadLocal<Counter, Counter>().value = 7; });
    Require(!retryThrew, "HostThreadLocal kept failing after fiber-local slots were freed");
    Require(HostThreadLocal<Counter, Counter>().value == 7, "HostThreadLocal lost its value after a retried slot allocation");
    retryThrew = Throws([&] { timer = PreciseWaitThreadTimer(); });
    Require(!retryThrew && timer != nullptr, "the precise wait timer stayed without a handle after fiber-local slots were freed");

    const auto timedStart = std::chrono::steady_clock::now();
    PreciseSleepUs(2000);
    const auto timed = std::chrono::steady_clock::now() - timedStart;
    Require(timed >= std::chrono::milliseconds(2), "a precise sleep after the retry returned early");
    std::puts("a failed fiber-local slot allocation is retried, and precise sleeps fall back meanwhile");
    return 0;
}
