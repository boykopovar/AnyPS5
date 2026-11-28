#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

using Callback = void (APS5_VABI *)(void*);

static void Require(bool condition) { if (!condition) std::abort(); }

static std::atomic<int> arrived{0};
static std::atomic<int> calls{0};
static std::uint64_t nestedFlag = 0;
static int nestedValue = 0;
static int published = 0;
static int throws = 0;

static void CallOnce(std::uint64_t* flag, void* argument, Callback callback) {
    _ZNSt3__111__call_onceERVmPvPFvS2_E_nid_postfix(flag, argument, callback);
}

static void APS5_VABI Nested(void* argument) {
    nestedValue = *static_cast<int*>(argument);
}

static void APS5_VABI Initialize(void* argument) {
    ++calls;
    while (arrived.load() != 8) std::this_thread::yield();
    CallOnce(&nestedFlag, argument, Nested);
    published = 42;
}

static void APS5_VABI ThrowThenSucceed(void*) {
    if (++throws == 1) throw 7;
}

static void APS5_VABI Unreachable(void*) {
    std::abort();
}

template<typename Predicate>
static void WaitFor(Predicate ready, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            std::fputs(message, stderr);
            std::fputc('\n', stderr);
            std::abort();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct ExceptionWaiters {
    std::atomic<bool> entered{false};
    std::atomic<bool> releaseThrow{false};
    std::atomic<bool> caught{false};
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> retries{0};
    int value = 0;
};

static void APS5_VABI ThrowWithWaiters(void* argument) {
    auto& state = *static_cast<ExceptionWaiters*>(argument);
    state.entered.store(true, std::memory_order_release);
    WaitFor([&] { return state.releaseThrow.load(std::memory_order_acquire); },
        "call_once throwing initializer was not released");
    throw 19;
}

static void APS5_VABI RetryWithWaiters(void* argument) {
    auto& state = *static_cast<ExceptionWaiters*>(argument);
    ++state.retries;
    state.value = 37;
}

static void CheckExceptionWaiters() {
    std::uint64_t flag = 0;
    ExceptionWaiters state;
    std::thread initializer([&] {
        try { CallOnce(&flag, &state, ThrowWithWaiters); std::abort(); }
        catch (int value) {
            Require(value == 19);
            state.caught.store(true, std::memory_order_release);
        }
    });
    WaitFor([&] { return state.entered.load(std::memory_order_acquire); },
        "call_once throwing initializer did not start");
    std::array<std::thread, 2> waiters;
    for (auto& waiter : waiters) waiter = std::thread([&] {
        state.started.fetch_add(1, std::memory_order_release);
        CallOnce(&flag, &state, RetryWithWaiters);
        Require(state.value == 37);
        state.finished.fetch_add(1, std::memory_order_release);
    });
    WaitFor([&] { return state.started.load(std::memory_order_acquire) == 2; },
        "call_once waiting callers did not start");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(state.finished.load(std::memory_order_acquire) == 0);
    state.releaseThrow.store(true, std::memory_order_release);
    WaitFor([&] { return state.finished.load(std::memory_order_acquire) == 2; },
        "call_once waiting callers did not retry after exception");
    initializer.join();
    for (auto& waiter : waiters) waiter.join();
    Require(state.caught.load(std::memory_order_acquire));
    Require(state.retries == 1 && flag == UINT64_MAX && state.value == 37);
}

int main() {
    std::uint64_t flag = 0;
    int token = 17;
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        ++arrived;
        CallOnce(&flag, &token, Initialize);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && flag == UINT64_MAX && nestedFlag == UINT64_MAX);
    CallOnce(&flag, nullptr, Unreachable);
    Require(calls == 1);

    std::uint64_t throwingFlag = 0;
    try { CallOnce(&throwingFlag, nullptr, ThrowThenSucceed); std::abort(); }
    catch (int value) { Require(value == 7 && throwingFlag == 0); }
    CallOnce(&throwingFlag, nullptr, ThrowThenSucceed);
    Require(throws == 2 && throwingFlag == UINT64_MAX);

    CheckExceptionWaiters();

    std::uint64_t alreadyComplete = UINT64_MAX;
    CallOnce(&alreadyComplete, nullptr, nullptr);
    std::uint64_t unknownState = 5;
    CallOnce(&unknownState, nullptr, Unreachable);
    Require(unknownState == 5);

    errno = ESRCH;
    try { CallOnce(nullptr, nullptr, Unreachable); std::abort(); }
    catch (const std::invalid_argument&) { Require(errno == ESRCH); }
    try { CallOnce(reinterpret_cast<std::uint64_t*>(1), nullptr, Unreachable); std::abort(); }
    catch (const std::invalid_argument&) { Require(errno == ESRCH); }
    std::uint64_t nullCallback = 0;
    try { CallOnce(&nullCallback, nullptr, nullptr); std::abort(); }
    catch (const std::invalid_argument&) { Require(nullCallback == 0); }
}
