#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;

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

static void RequireAlive(Pthread thread) {
    char name[32];
    int priority = 0;
    KernelCpumask mask = 0;
    Require(scePthreadGetname(thread, name) == SCE_OK);
    Require(scePthreadGetprio(thread, &priority) == SCE_OK);
    Require(scePthreadGetaffinity(thread, &mask) == SCE_OK && mask != 0);
    Require(scePthreadSetaffinity(thread, mask) == SCE_OK);
    Require(scePthreadRename(thread, "renamed") == SCE_OK);
}

static void RequireEnded(Pthread thread) {
    char name[32];
    int priority = 0;
    KernelCpumask mask = 0;
    Require(scePthreadGetname(thread, name) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadGetprio(thread, &priority) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadGetaffinity(thread, &mask) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadSetaffinity(thread, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadRename(thread, "renamed") == SCE_KERNEL_ERROR_ESRCH);
}

static void WaitUntilReleased(Pthread thread) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int priority = 0;
    while (scePthreadGetprio(thread, &priority) != SCE_KERNEL_ERROR_ESRCH) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

int main() {
    RequireAlive(scePthreadSelf());

    for (int round = 0; round < 20; ++round) {
        Worker detached;
        Pthread detachedThread = nullptr;
        Require(scePthreadCreate(&detachedThread, nullptr, Wait, &detached, "detached") == SCE_OK);
        while (!detached.started.load()) std::this_thread::yield();
        RequireAlive(detachedThread);
        Require(scePthreadDetach(detachedThread) == SCE_OK);
        detached.release.store(true);
        WaitUntilReleased(detachedThread);
        RequireEnded(detachedThread);
    }
    return 0;
}
