#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>
#include <thread>

struct UsbdTimeval { std::int64_t seconds; std::int64_t microseconds; };

extern "C" {
int APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
int APS5_VABI sceUsbdTryLockEvents();
void APS5_VABI sceUsbdLockEvents();
void APS5_VABI sceUsbdUnlockEvents();
int APS5_VABI sceUsbdEventHandlerActive();
int APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdLockEventWaiters();
void APS5_VABI sceUsbdUnlockEventWaiters();
int APS5_VABI sceUsbdWaitForEvent(UsbdTimeval*);
int APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval*);
int APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval*);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}

void Locks() {
    Require(sceUsbdEventHandlerActive() == 0, "initial event handler is active");
    Require(sceUsbdEventHandlingOk() == 0, "non-owner allowed to handle events");
    Require(sceUsbdTryLockEvents() == 0, "initial event try-lock failed");
    Require(sceUsbdEventHandlerActive() == 1 && sceUsbdEventHandlingOk() == 1, "event lock did not activate handler");
    std::thread contender([] {
        Require(sceUsbdTryLockEvents() == 1, "contender acquired held event lock");
        Require(sceUsbdEventHandlingOk() == 0, "non-owner reported event handling OK");
    });
    contender.join();
    UsbdTimeval immediate{};
    Require(sceUsbdHandleEventsLocked(&immediate) == 0, "locked event pump failed");
    sceUsbdUnlockEvents();
    Require(sceUsbdEventHandlerActive() == 0, "unlock left handler active");
    std::thread next([] {
        sceUsbdLockEvents();
        Require(sceUsbdEventHandlerActive() == 1, "blocking lock did not activate handler");
        sceUsbdUnlockEvents();
    });
    next.join();
}

void Waiters() {
    sceUsbdLockEventWaiters();
    UsbdTimeval immediate{};
    Require(sceUsbdWaitForEvent(&immediate) == 1, "wait timeout lost positive timeout result");
    Require(sceUsbdEventHandlingOk() == 1, "timed-out waiter did not take vacant handler role");
    sceUsbdUnlockEvents();
    std::promise<void> ready;
    auto waiting = ready.get_future();
    std::thread waiter([&] {
        sceUsbdLockEventWaiters();
        ready.set_value();
        UsbdTimeval limit{2, 0};
        Require(sceUsbdWaitForEvent(&limit) == 0, "event unlock did not wake waiter");
        Require(sceUsbdEventHandlingOk() == 1, "woken waiter did not take handler role");
        sceUsbdUnlockEvents();
    });
    waiting.wait();
    sceUsbdLockEventWaiters();
    sceUsbdUnlockEventWaiters();
    sceUsbdLockEvents();
    sceUsbdUnlockEvents();
    waiter.join();
}

void Timeouts() {
    constexpr int invalid = static_cast<int>(0x80240002);
    for (UsbdTimeval timeout : {UsbdTimeval{-1, 0}, {0, -1}, {0, 1000000}}) {
        Require(sceUsbdHandleEventsTimeout(&timeout) == invalid, "ordinary pump accepted invalid timeout");
        int rejected = 0;
        try { sceUsbdHandleEventsLocked(&timeout); } catch (const std::runtime_error&) { ++rejected; }
        try { sceUsbdWaitForEvent(&timeout); } catch (const std::runtime_error&) { ++rejected; }
        Require(rejected == 2, "unverified timeout result was fabricated");
    }
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalid, "timeout pump accepted null timeout");
#ifdef _WIN32
    UsbdTimeval huge{std::numeric_limits<std::int64_t>::max(), 0};
    int rejected = 0;
    try { sceUsbdHandleEventsTimeout(&huge); } catch (const std::runtime_error&) { ++rejected; }
    try { sceUsbdHandleEventsLocked(&huge); } catch (const std::runtime_error&) { ++rejected; }
    try { sceUsbdWaitForEvent(&huge); } catch (const std::runtime_error&) { ++rejected; }
    Require(rejected == 3, "64-bit guest timeout silently narrowed on Windows");
#endif
    UsbdTimeval immediate{};
    Require(sceUsbdHandleEventsTimeout(&immediate) == 0, "zero timeout pump failed");
}

void SleepingExit() {
    std::promise<void> ready;
    auto waiting = ready.get_future();
    std::thread waiter([&] {
        sceUsbdLockEventWaiters();
        ready.set_value();
        UsbdTimeval limit{5, 0};
        Require(sceUsbdWaitForEvent(&limit) == 0, "sleeping waiter was not woken");
        Require(sceUsbdEventHandlingOk() == 1, "waiter lost initialized context");
        sceUsbdUnlockEvents();
    });
    waiting.wait();
    sceUsbdLockEventWaiters();
    sceUsbdUnlockEventWaiters();
    bool rejected = false;
    try { sceUsbdExit(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "exit destroyed context beneath a sleeping waiter");
    sceUsbdLockEvents();
    sceUsbdUnlockEvents();
    waiter.join();
    Require(sceUsbdTryLockEvents() == 0, "rejected exit consumed initialization reference");
    sceUsbdUnlockEvents();
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected event test mode");
    Require(sceUsbdTryLockEvents() == 1, "uninitialized context acquired event lock");
    Require(sceUsbdInit() == 0, "USB initialization failed");
    if (std::strcmp(argv[1], "locks") == 0) Locks();
    else if (std::strcmp(argv[1], "waiters") == 0) Waiters();
    else if (std::strcmp(argv[1], "timeouts") == 0) Timeouts();
    else if (std::strcmp(argv[1], "sleeping-exit") == 0) SleepingExit();
    else Require(false, "unknown event test mode");
    sceUsbdExit();
    Require(sceUsbdTryLockEvents() == 1, "exited context acquired event lock");
    bool rejected = false;
    try { sceUsbdLockEvents(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "missing context silently used host default context");
}
