#include "prx/libc/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void RequireAt(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cxa guard check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) RequireAt((value), __LINE__)

static bool Initialized(const std::uint64_t& guard) {
    return (guard & 1) != 0;
}

static void WaitForStartAt(const std::atomic<bool>& started, int line) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!started && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    RequireAt(started, line);
}
#define WaitForStart(started) WaitForStartAt((started), __LINE__)

int main() {
    {
        std::uint64_t guard = 0;
        Require(__cxa_guard_acquire_nid_postfix(&guard) != 0);
        Require(!Initialized(guard));
        __cxa_guard_release_nid_postfix(&guard);
        Require(Initialized(guard));
        Require(__cxa_guard_acquire_nid_postfix(&guard) == 0);
        Require(Initialized(guard));
    }

    {
        std::uint64_t guard = 0;
        Require(__cxa_guard_acquire_nid_postfix(&guard) != 0);
        __cxa_guard_abort_nid_postfix(&guard);
        Require(!Initialized(guard));
        Require(__cxa_guard_acquire_nid_postfix(&guard) != 0);
        __cxa_guard_release_nid_postfix(&guard);
        Require(Initialized(guard));
        Require(__cxa_guard_acquire_nid_postfix(&guard) == 0);
    }

    {
        std::uint64_t outer = 0;
        std::uint64_t inner = 0;
        Require(__cxa_guard_acquire_nid_postfix(&outer) != 0);
        Require(__cxa_guard_acquire_nid_postfix(&inner) != 0);
        __cxa_guard_release_nid_postfix(&inner);
        Require(Initialized(inner) && !Initialized(outer));
        __cxa_guard_release_nid_postfix(&outer);
        Require(Initialized(outer));
    }

    {
        std::uint64_t guard = 0;
        Require(__cxa_guard_acquire_nid_postfix(&guard) != 0);
        std::atomic<bool> started{false};
        std::atomic<bool> returned{false};
        std::atomic<int> result{-1};
        std::thread waiter([&] {
            started = true;
            result = __cxa_guard_acquire_nid_postfix(&guard);
            returned = true;
        });
        WaitForStart(started);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Require(!returned);
        __cxa_guard_release_nid_postfix(&guard);
        waiter.join();
        Require(returned && result == 0);
        Require(Initialized(guard));
    }

    {
        std::uint64_t guard = 0;
        Require(__cxa_guard_acquire_nid_postfix(&guard) != 0);
        std::atomic<bool> started{false};
        std::atomic<bool> returned{false};
        std::atomic<int> result{-1};
        std::thread waiter([&] {
            started = true;
            result = __cxa_guard_acquire_nid_postfix(&guard);
            returned = true;
            if (result != 0) __cxa_guard_release_nid_postfix(&guard);
        });
        WaitForStart(started);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Require(!returned);
        __cxa_guard_abort_nid_postfix(&guard);
        waiter.join();
        Require(returned && result != 0);
        Require(Initialized(guard));
    }

    return 0;
}
