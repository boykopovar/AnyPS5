#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

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

static void WaitForFinalRelease() {
    std::unique_lock lock(gate.mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    Require(gate.changed.wait_until(lock, deadline, [] { return gate.reached; }));
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
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

static void RequireAdmissionRejectsFinalRelease(int operation) {
    Worker worker;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Wait, &worker, "release_race") == SCE_OK);
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

int main() {
    for (int round = 0; round < 20; ++round) {
        RequireAdmissionRejectsFinalRelease(0);
        RequireAdmissionRejectsFinalRelease(1);
        RequireAdmissionRejectsFinalRelease(2);
    }
    return 0;
}
