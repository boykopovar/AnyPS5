#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/RewindKick.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/LeanFront.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SegmentLoop.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/WaitMemory.hpp"
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::int64_t FarAheadNs = 10 * NanosecondsPerSecond;

#ifdef _WIN32
std::int64_t ticksPerSecond() {
    static const std::int64_t frequency = [] {
        LARGE_INTEGER value{};
        return QueryPerformanceFrequency(&value) && value.QuadPart > 0 ? static_cast<std::int64_t>(value.QuadPart) : std::int64_t{0};
    }();
    return frequency;
}
#endif

}

std::int64_t FlushTicks() {
#ifdef _WIN32
    if (ticksPerSecond() != 0) {
        LARGE_INTEGER value{};
        QueryPerformanceCounter(&value);
        return static_cast<std::int64_t>(value.QuadPart);
    }
#endif
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t FlushTicksAt(std::chrono::steady_clock::time_point at) {
#ifdef _WIN32
    const auto frequency = ticksPerSecond();
    if (frequency != 0) {
        const auto now = FlushTicks();
        const auto ahead = std::chrono::duration_cast<std::chrono::nanoseconds>(at - std::chrono::steady_clock::now()).count();
        const auto margin = frequency / 1'000'000 > 0 ? frequency / 1'000'000 : std::int64_t{1};
        if (ahead <= 0) return now - margin;
        const auto clamped = ahead < FarAheadNs ? ahead : FarAheadNs;
        const auto ticks = (clamped / NanosecondsPerSecond) * frequency + ((clamped % NanosecondsPerSecond) * frequency + NanosecondsPerSecond - 1) / NanosecondsPerSecond;
        return now + ticks - margin;
    }
#endif
    return std::chrono::duration_cast<std::chrono::nanoseconds>(at.time_since_epoch()).count();
}

RewindKick& RewindKick::Get() {
    static RewindKick* kick = new RewindKick();
    return *kick;
}

RewindKick::RewindKick() {
#ifdef _WIN32
    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
}

void RewindKick::signal() {
#ifdef _WIN32
    if (event != nullptr) SetEvent(static_cast<HANDLE>(event));
#else
    {
        std::lock_guard lock(mutex);
        ++signals;
    }
    changed.notify_all();
#endif
}

void RewindKick::Kick(const std::uint32_t* control) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (waiters.load(std::memory_order_acquire) == 0) {
        LeanFront::Add(LeanFront::Counter::KicksWithoutWaiter);
        return;
    }
    if (awaited.load(std::memory_order_acquire) == control) signal();
}

void RewindKick::WakeIdle() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (idle.load(std::memory_order_acquire)) signal();
}

RewindKick::Wake RewindKick::sleep(std::uint64_t& seen) {
#ifdef _WIN32
    static_cast<void>(seen);
    if (event != nullptr && timer != nullptr) {
        LARGE_INTEGER due{};
        due.QuadPart = -2000;
        if (SetWaitableTimer(static_cast<HANDLE>(timer), &due, 0, nullptr, nullptr, FALSE)) {
            const HANDLE handles[2]{static_cast<HANDLE>(event), static_cast<HANDLE>(timer)};
            return WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 ? Wake::Kick : Wake::Timer;
        }
    }
    PollSleep();
    return Wake::Timer;
#else
    std::unique_lock lock(mutex);
    const bool kicked = changed.wait_for(lock, std::chrono::microseconds(200), [&] { return signals != seen; });
    seen = signals;
    return kicked ? Wake::Kick : Wake::Timer;
#endif
}

bool RewindKick::SleepIdle(const std::atomic<std::uint64_t>& queued) {
    std::uint64_t seen = 0;
#ifndef _WIN32
    {
        std::lock_guard lock(mutex);
        seen = signals;
    }
#endif
    idle.store(true, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const bool kicked = queued.load(std::memory_order_acquire) == 0 && sleep(seen) == Wake::Kick;
    idle.store(false, std::memory_order_release);
    return kicked;
}

RewindKick::Waiter::Waiter(RewindKick& owner, const std::uint32_t* control) : kick(owner) {
#ifndef _WIN32
    {
        std::lock_guard lock(kick.mutex);
        seen = kick.signals;
    }
#endif
    kick.awaited.store(control, std::memory_order_release);
    kick.waiters.fetch_add(1, std::memory_order_seq_cst);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

RewindKick::Waiter::~Waiter() {
    kick.waiters.fetch_sub(1, std::memory_order_acq_rel);
    kick.awaited.store(nullptr, std::memory_order_release);
}

RewindKick::Wake RewindKick::Waiter::Sleep() {
    return kick.sleep(seen);
}

}
