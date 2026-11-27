#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int ROUNDS = 20;
static constexpr int RACE_ROUNDS = 200;
static constexpr int RACE_DELAY_STEPS = 40;
static constexpr int RACE_DELAY_SPINS = 500;
static constexpr auto RACE_WINDOW = std::chrono::milliseconds(2);
static constexpr auto STOPPED_WINDOW = std::chrono::milliseconds(10);
static constexpr auto PROGRESS_LIMIT = std::chrono::seconds(5);
static void* const FINISHED = reinterpret_cast<void*>(std::uintptr_t{7});

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template <typename TAction>
static bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct Worker {
    std::atomic<std::uint64_t> progress{0};
    std::atomic<bool> stop{false};
};

static void* APS5_VABI Spin(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->stop.load()) worker->progress.fetch_add(1);
    return FINISHED;
}

static void* APS5_VABI Nap(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->stop.load()) {
        sceKernelUsleep_nid_postfix(200);
        worker->progress.fetch_add(1);
    }
    return FINISHED;
}

struct SuspendRequest {
    Pthread target = nullptr;
    std::atomic<bool> go{false};
};

static void* APS5_VABI SuspendTarget(void* opaque) {
    auto* request = static_cast<SuspendRequest*>(opaque);
    while (!request->go.load()) {}
    Require(scePthreadSuspend(request->target) == SCE_OK, "a concurrent scePthreadSuspend did not return SCE_OK");
    return FINISHED;
}

static void* APS5_VABI Record(void* opaque) {
    static_cast<Worker*>(opaque)->progress.fetch_add(1);
    return FINISHED;
}

static void WaitForProgress(const Worker& worker, std::uint64_t after, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + PROGRESS_LIMIT;
    while (worker.progress.load() <= after) {
        Require(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::yield();
    }
}

static Pthread Start(PthreadEntry entry, Worker& worker, const char* name) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, &worker, name) == SCE_OK, "scePthreadCreate failed");
    return thread;
}

static void Finish(Pthread thread, Worker& worker) {
    worker.stop.store(true);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == FINISHED, "joining a worker failed");
}

template <std::size_t TCount>
static std::array<std::uint64_t, TCount> RequireStopped(const std::array<Worker, TCount>& workers, const char* message) {
    std::array<std::uint64_t, TCount> stopped{};
    for (std::size_t index = 0; index < TCount; ++index) stopped[index] = workers[index].progress.load();
    std::this_thread::sleep_for(STOPPED_WINDOW);
    for (std::size_t index = 0; index < TCount; ++index) Require(workers[index].progress.load() == stopped[index], message);
    return stopped;
}

template <std::size_t TCount>
static void RequireRunning(const std::array<Worker, TCount>& workers, const std::array<std::uint64_t, TCount>& stopped, const char* message) {
    for (std::size_t index = 0; index < TCount; ++index) WaitForProgress(workers[index], stopped[index], message);
}

static void SuspendStopsOneThread() {
    std::array<Worker, 2> workers;
    const std::array<Pthread, 2> threads{Start(Spin, workers[0], "spin"), Start(Nap, workers[1], "nap")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    for (int round = 0; round < ROUNDS; ++round) {
        for (std::size_t index = 0; index < threads.size(); ++index) {
            Require(scePthreadSuspend(threads[index]) == SCE_OK, "scePthreadSuspend did not return SCE_OK");
            const std::uint64_t stopped = workers[index].progress.load();
            const std::uint64_t other = workers[1 - index].progress.load();
            std::this_thread::sleep_for(STOPPED_WINDOW);
            Require(workers[index].progress.load() == stopped, "a thread kept running after scePthreadSuspend");
            WaitForProgress(workers[1 - index], other, "scePthreadSuspend stopped another thread");
            Require(scePthreadResume(threads[index]) == SCE_OK, "scePthreadResume failed");
            WaitForProgress(workers[index], stopped, "a thread did not continue after scePthreadResume");
        }
    }
    for (std::size_t index = 0; index < threads.size(); ++index) Finish(threads[index], workers[index]);
}

static void SuspendRightAfterResumeStops() {
    std::array<Worker, 2> workers;
    const std::array<Pthread, 2> threads{Start(Spin, workers[0], "spin"), Start(Nap, workers[1], "nap")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    for (std::size_t index = 0; index < threads.size(); ++index) {
        Require(scePthreadSuspend(threads[index]) == SCE_OK, "scePthreadSuspend did not return SCE_OK");
        for (int round = 0; round < RACE_ROUNDS; ++round) {
            Require(scePthreadResume(threads[index]) == SCE_OK, "scePthreadResume failed");
            for (volatile int spin = 0; spin < round % RACE_DELAY_STEPS * RACE_DELAY_SPINS; spin = spin + 1) {}
            Require(scePthreadSuspend(threads[index]) == SCE_OK, "scePthreadSuspend did not return SCE_OK");
            const std::uint64_t stopped = workers[index].progress.load();
            std::this_thread::sleep_for(RACE_WINDOW);
            Require(workers[index].progress.load() == stopped, "a thread suspended right after scePthreadResume kept running");
        }
        const std::uint64_t stopped = workers[index].progress.load();
        Require(scePthreadResume(threads[index]) == SCE_OK, "scePthreadResume failed");
        WaitForProgress(workers[index], stopped, "a thread did not continue after scePthreadResume");
    }
    for (std::size_t index = 0; index < threads.size(); ++index) Finish(threads[index], workers[index]);
}

static void ConcurrentSuspendsResumeOnce() {
    std::array<Worker, 1> workers;
    const Pthread spin = Start(Spin, workers[0], "spin");
    WaitForProgress(workers[0], 0, "the spinner never started");
    for (int round = 0; round < RACE_ROUNDS; ++round) {
        SuspendRequest request;
        request.target = spin;
        std::array<Pthread, 2> suspenders{};
        for (auto& suspender : suspenders) Require(scePthreadCreate(&suspender, nullptr, SuspendTarget, &request, "suspender") == SCE_OK, "scePthreadCreate failed");
        request.go.store(true);
        for (auto suspender : suspenders) Require(scePthreadJoin(suspender, nullptr) == SCE_OK, "joining a suspending thread failed");
        const std::uint64_t stopped = workers[0].progress.load();
        std::this_thread::sleep_for(RACE_WINDOW);
        Require(workers[0].progress.load() == stopped, "a thread kept running after two concurrent scePthreadSuspend calls");
        Require(scePthreadResume(spin) == SCE_OK, "scePthreadResume failed");
        WaitForProgress(workers[0], stopped, "a thread suspended by two concurrent calls did not continue after one scePthreadResume");
    }
    Finish(spin, workers[0]);
}

static void SuspendErrorsThrow() {
    Worker recorded;
    const Pthread joined = Start(Record, recorded, "joined");
    Require(scePthreadJoin(joined, nullptr) == SCE_OK, "scePthreadJoin failed");
    Require(Throws([joined] { scePthreadSuspend(joined); }), "scePthreadSuspend of a joined thread did not throw");
    std::array<Worker, 1> workers;
    const Pthread spin = Start(Spin, workers[0], "spin");
    WaitForProgress(workers[0], 0, "the spinner never started");
    Require(Throws([] { scePthreadSuspend(nullptr); }), "scePthreadSuspend of a null thread did not throw");
    Require(Throws([] { scePthreadSuspend(scePthreadSelf()); }), "scePthreadSuspend of the calling thread did not throw");
    const std::uint64_t before = workers[0].progress.load();
    WaitForProgress(workers[0], before, "a failed scePthreadSuspend stopped another thread");
    Finish(spin, workers[0]);
}

static void SuspendAllStopsEveryOtherThread() {
    std::array<Worker, 2> workers;
    const std::array<Pthread, 2> threads{Start(Spin, workers[0], "spin"), Start(Nap, workers[1], "nap")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    for (int round = 0; round < ROUNDS; ++round) {
        scePthreadSuspendAll();
        const auto stopped = RequireStopped(workers, "a thread kept running after scePthreadSuspendAll");
        scePthreadResumeAll();
        RequireRunning(workers, stopped, "a thread did not continue after scePthreadResumeAll");
    }
    for (std::size_t index = 0; index < threads.size(); ++index) Finish(threads[index], workers[index]);
}

static void SingleStopsEveryOtherThread() {
    std::array<Worker, 2> workers;
    const std::array<Pthread, 2> threads{Start(Spin, workers[0], "spin"), Start(Nap, workers[1], "nap")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    for (int round = 0; round < ROUNDS; ++round) {
        Require(scePthreadSingle() == SCE_OK, "scePthreadSingle did not return SCE_OK");
        const auto stopped = RequireStopped(workers, "a thread kept running after scePthreadSingle");
        Require(scePthreadMulti() == SCE_OK, "scePthreadMulti failed");
        RequireRunning(workers, stopped, "a thread did not continue after scePthreadMulti");
    }
    for (std::size_t index = 0; index < threads.size(); ++index) Finish(threads[index], workers[index]);
}

static void SuspendedThreadStaysStoppedThroughSuspendAll() {
    std::array<Worker, 2> workers;
    const std::array<Pthread, 2> threads{Start(Spin, workers[0], "early"), Start(Spin, workers[1], "late")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    Require(scePthreadSuspend(threads[0]) == SCE_OK, "scePthreadSuspend did not return SCE_OK");
    scePthreadSuspendAll();
    const auto stopped = RequireStopped(workers, "a thread kept running after scePthreadSuspendAll");
    scePthreadResumeAll();
    RequireRunning(workers, stopped, "a thread did not continue after scePthreadResumeAll");
    for (std::size_t index = 0; index < threads.size(); ++index) Finish(threads[index], workers[index]);
}

int main() {
    SuspendStopsOneThread();
    SuspendRightAfterResumeStops();
    ConcurrentSuspendsResumeOnce();
    SuspendErrorsThrow();
    SuspendAllStopsEveryOtherThread();
    SingleStopsEveryOtherThread();
    SuspendedThreadStaysStoppedThroughSuspendAll();
    std::puts("guest_sce_pthread_suspend: ok");
    return 0;
}
