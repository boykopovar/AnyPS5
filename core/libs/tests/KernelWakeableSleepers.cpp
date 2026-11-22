#include "prx/libkernel/Time/include/SleepWake.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    Require(KernelWakeableSleepers_nid_no_patch() == 0, "a wakeable sleeper was counted before any thread slept");
    std::atomic<bool> done{false};
    const auto start = std::chrono::steady_clock::now();
    std::thread sleeper([&] {
        SleepWake::Sleep(SleepWake::MAX_WAKEABLE_NANOS);
        done.store(true, std::memory_order_release);
    });
    bool counted = false;
    while (!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
        if (KernelWakeableSleepers_nid_no_patch() == 1) {
            counted = true;
            KernelWakeSleepers_nid_no_patch();
            break;
        }
    }
    sleeper.join();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(counted, "a thread in a wakeable sleep was not counted");
    Require(elapsed < std::chrono::milliseconds(500), "the woken sleeper did not return");
    Require(KernelWakeableSleepers_nid_no_patch() == 0, "a sleeper stayed counted after it returned");
    std::puts("wakeable sleepers counted and released");
    return 0;
}
