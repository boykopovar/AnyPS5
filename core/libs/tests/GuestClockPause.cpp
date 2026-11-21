#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

static std::uint64_t MonotonicNanos() {
    KernelTimespec time{};
    Require(clock_gettime_nid_postfix(4, &time) == 0);
    return static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(time.tv_nsec);
}

static void PausedTimeIsNotCounted() {
    const auto before = sceKernelGetProcessTime();
    KernelPauseGuestClock_nid_no_patch();
    KernelPauseGuestClock_nid_no_patch();
    const auto paused = sceKernelGetProcessTime();
    const auto pausedNanos = MonotonicNanos();
    Require(paused >= before);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    KernelResumeGuestClock_nid_no_patch();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(sceKernelGetProcessTime() == paused);
    Require(MonotonicNanos() == pausedNanos);
    KernelResumeGuestClock_nid_no_patch();
    const auto resumed = sceKernelGetProcessTime();
    Require(resumed >= paused && resumed - paused < 30000);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    Require(sceKernelGetProcessTime() - resumed >= 15000);
    KernelResumeGuestClock_nid_no_patch();
    const auto unbalanced = sceKernelGetProcessTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(sceKernelGetProcessTime() > unbalanced);
}

static void ClockStaysMonotonicWhileToggled() {
    std::atomic<bool> stop{false};
    std::atomic<bool> backwards{false};
    std::thread reader([&] {
        auto last = MonotonicNanos();
        while (!stop.load()) {
            const auto now = MonotonicNanos();
            if (now < last) backwards.store(true);
            last = now;
        }
    });
    for (unsigned i = 0; i < 2000; ++i) {
        KernelPauseGuestClock_nid_no_patch();
        std::this_thread::yield();
        KernelResumeGuestClock_nid_no_patch();
    }
    stop.store(true);
    reader.join();
    Require(!backwards.load());
}

int main() {
    PausedTimeIsNotCounted();
    ClockStaysMonotonicWhileToggled();
    return 0;
}
