#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

static constexpr int SCE_OK = 0;

extern "C" void APS5_Test_SetFinalThreadReleaseHook(Pthread thread, void (*hook)(Pthread));

static void Require(bool condition) { if (!condition) std::abort(); }

struct Worker {
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
};

static void* APS5_VABI Wait(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    worker->started.store(true);
    while (!worker->release.load()) std::this_thread::yield();
    return nullptr;
}

struct FinalReleaseGate {
    std::mutex mutex;
    std::condition_variable changed;
    Pthread thread = nullptr;
    bool reached = false;
    bool release = false;
};

static FinalReleaseGate gate;

static void HoldFinalRelease(Pthread thread) {
    std::unique_lock lock(gate.mutex);
    Require(gate.thread == thread);
    gate.reached = true;
    gate.changed.notify_all();
    gate.changed.wait(lock, [] { return gate.release; });
}

static bool WaitForFinalRelease() {
    std::unique_lock lock(gate.mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const bool reached = gate.changed.wait_until(lock, deadline, [] { return gate.reached; });
    if (!gate.release) {
        gate.release = true;
        gate.changed.notify_all();
    }
    return reached;
}

static void ResumeFinalRelease() {
    {
        std::lock_guard lock(gate.mutex);
        gate.release = true;
    }
    gate.changed.notify_all();
}

static void WaitUntilReleased(Pthread thread) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int priority = 0;
    while (scePthreadGetprio(thread, &priority) != SCE_KERNEL_ERROR_ESRCH) {
        if (std::chrono::steady_clock::now() >= deadline) return;
        std::this_thread::yield();
    }
}

static std::atomic<bool> finalReleaseSeen{false};

static void HoldFinalReleaseMarked(Pthread thread) {
    finalReleaseSeen.store(true, std::memory_order_release);
    HoldFinalRelease(thread);
}

static bool WorkerFinished(Pthread thread) {
    return thread->_finished.load(std::memory_order_acquire);
}

static void RequireJoinDetachInterleave() {
    Worker worker;
    Pthread thread = nullptr;
    {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, 64 * 1024) == SCE_OK);
        Require(scePthreadCreate(&thread, &attr, Wait, &worker, "join_detach_race") == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
    while (!worker.started.load()) std::this_thread::yield();
    {
        std::lock_guard lock(gate.mutex);
        gate.thread = thread;
        gate.reached = false;
        gate.release = false;
    }
    finalReleaseSeen.store(false, std::memory_order_relaxed);
    APS5_Test_SetFinalThreadReleaseHook(thread, HoldFinalReleaseMarked);

    std::atomic<int> joinersDone{0};
    std::atomic<bool> racerDone{false};
    std::atomic<int> detachResult{SCE_OK};

    std::thread racer([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!WorkerFinished(thread)) {
            if (std::chrono::steady_clock::now() >= deadline) break;
            std::this_thread::yield();
        }
        detachResult.store(scePthreadDetach(thread), std::memory_order_release);
        racerDone.store(true, std::memory_order_release);
    });

    std::thread joiners[2];
    for (auto& joiner : joiners) {
        joiner = std::thread([&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            KernelTimespec past{};
            for (;;) {
                const int result = pthread_timedjoin_np_nid_postfix(thread, nullptr, &past);
                if (result == 0 || result == EINVAL) break;
                if (result == ESRCH) {
                    if (finalReleaseSeen.load(std::memory_order_acquire)) WaitForFinalRelease();
                    break;
                }
                if (std::chrono::steady_clock::now() >= deadline) break;
                std::this_thread::yield();
            }
            joinersDone.fetch_add(1, std::memory_order_release);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    worker.release.store(true, std::memory_order_release);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    bool resumed = false;
    while (joinersDone.load(std::memory_order_acquire) < 2 || !racerDone.load(std::memory_order_acquire)) {
        if (!resumed && finalReleaseSeen.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            ResumeFinalRelease();
            resumed = true;
        }
        if (std::chrono::steady_clock::now() >= deadline) break;
        std::this_thread::yield();
    }
    for (auto& joiner : joiners) joiner.join();
    racer.join();
    const int detach = detachResult.load(std::memory_order_acquire);
    Require(detach == SCE_OK || detach == SCE_KERNEL_ERROR_ESRCH || detach == SCE_KERNEL_ERROR_EINVAL);
    if (resumed) WaitUntilReleased(thread);
    else APS5_Test_SetFinalThreadReleaseHook(thread, nullptr);
}

static void RequireAdmissionRejectsFinalRelease(int operation) {
    Worker worker;
    Pthread thread = nullptr;
    {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, 64 * 1024) == SCE_OK);
        Require(scePthreadCreate(&thread, &attr, Wait, &worker, "release_race") == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
    while (!worker.started.load()) std::this_thread::yield();
    Require(scePthreadDetach(thread) == SCE_OK);

    {
        std::lock_guard lock(gate.mutex);
        gate.thread = thread;
        gate.reached = false;
        gate.release = false;
    }
    APS5_Test_SetFinalThreadReleaseHook(thread, HoldFinalRelease);
    worker.release.store(true);
    WaitForFinalRelease();

    int result = SCE_OK;
    if (operation == 0) result = scePthreadJoin(thread, nullptr);
    else if (operation == 1) result = scePthreadDetach(thread);
    else result = scePthreadCancel(thread);

    ResumeFinalRelease();
    WaitUntilReleased(thread);
    Require(result == SCE_KERNEL_ERROR_ESRCH);
}

static constexpr int PaddingCount = 1000;

static void* APS5_VABI Pad(void*) {
    return nullptr;
}

static void RequireJoinDetachInterleaves() {
    std::vector<Pthread> padding(PaddingCount);
    for (int index = 0; index < PaddingCount; ++index) {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, 64 * 1024) == SCE_OK);
        Require(scePthreadCreate(&padding[index], &attr, Pad, nullptr, "registry_padding") == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
    for (int round = 0; round < 60; ++round) RequireJoinDetachInterleave();
    for (int index = 0; index < PaddingCount; ++index)
        Require(scePthreadJoin(padding[index], nullptr) == SCE_OK);
}

int main() {
    for (int round = 0; round < 10; ++round) {
        RequireAdmissionRejectsFinalRelease(0);
        RequireAdmissionRejectsFinalRelease(1);
        RequireAdmissionRejectsFinalRelease(2);
    }
    RequireJoinDetachInterleaves();
    return 0;
}
