#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr unsigned long InitFailed = 4;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "init once: %s\n", message);
    std::abort();
}

int Begin(void** once) {
    int pending = -1;
    Require(__std_init_once_begin_initialize_clr_nid_postfix(once, 0, &pending, nullptr) == 1, "begin returns 1");
    return pending;
}

}

int main() {
    alignas(8) void* once = nullptr;
    Require(Begin(&once) == 1, "the first caller runs the initialization");
    Require(reinterpret_cast<std::uintptr_t>(once) == 1, "a running initialization marks the object");
    Require(__std_init_once_complete_clr_nid_postfix(&once, InitFailed, nullptr) == 1, "complete a failed initialization");
    Require(once == nullptr, "a failed initialization resets the object");
    Require(Begin(&once) == 1, "a caller after a failure runs the initialization again");

    std::atomic<bool> waiterDone{false};
    std::atomic<int> waiterPending{-1};
    std::thread waiter([&] {
        waiterPending = Begin(&once);
        waiterDone = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!waiterDone, "a second caller waits while the initialization runs");
    Require(__std_init_once_complete_clr_nid_postfix(&once, 0, nullptr) == 1, "complete the initialization");
    waiter.join();
    Require(waiterPending == 0, "the waiting caller sees the initialization done");
    Require(reinterpret_cast<std::uintptr_t>(once) == 2, "a completed initialization marks the object");
    Require(Begin(&once) == 0, "a later caller does not run the initialization");

    alignas(8) void* second = nullptr;
    for (int attempt = 0; attempt < 2; ++attempt) {
        Require(Begin(&second) == 1, "caller acquires initialization before failure");
        Require(__std_init_once_complete_clr_nid_postfix(&second, InitFailed, nullptr) == 1, "complete failed attempt");
        Require(second == nullptr, "repeated InitFailed resets the object");
    }
    Require(Begin(&second) == 1, "caller retries after repeated failures");
    Require(__std_init_once_complete_clr_nid_postfix(&second, 1, nullptr) == 1, "complete with flag 1");
    Require(reinterpret_cast<std::uintptr_t>(second) == 2, "flag 1 marks the object done");
    Require(Begin(&second) == 0, "done object skips re-initialization");

    alignas(8) void* groupOnce = nullptr;
    Require(Begin(&groupOnce) == 1, "leader begins group initialization");
    std::atomic<int> completedWaiters{0};
    int pendingResults[3] = {-1, -1, -1};
    std::thread waiters[3]{
        std::thread([&] {
            pendingResults[0] = Begin(&groupOnce);
            completedWaiters.fetch_add(1);
        }),
        std::thread([&] {
            pendingResults[1] = Begin(&groupOnce);
            completedWaiters.fetch_add(1);
        }),
        std::thread([&] {
            pendingResults[2] = Begin(&groupOnce);
            completedWaiters.fetch_add(1);
        }),
    };
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    Require(completedWaiters.load() == 0, "all group waiters block while initialization runs");
    Require(__std_init_once_complete_clr_nid_postfix(&groupOnce, 0x10, nullptr) == 1, "complete group initialization with non-zero flag");
    for (auto& thread : waiters) thread.join();
    Require(completedWaiters.load() == 3, "single completion wakes all waiters");
    for (const int pending : pendingResults) Require(pending == 0, "every group waiter sees initialization done");
    Require(reinterpret_cast<std::uintptr_t>(groupOnce) == 2, "group object stays marked done");
    return 0;
}
