#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int ROUNDS = 50;
static constexpr int DETACHED = 1;

static void Require(bool condition) { if (!condition) std::abort(); }

struct Worker {
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
};

static void* APS5_VABI Wait(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->release.load()) std::this_thread::yield();
    worker->finished.store(true);
    return nullptr;
}

int main() {
    for (int round = 0; round < ROUNDS; ++round) {
        Worker worker;
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, Wait, &worker, "detach_attr") == SCE_OK);
        std::atomic<bool> observing{true};
        std::atomic<bool> started{false};
        std::thread observer([&] {
            PthreadAttr attr = nullptr;
            Require(scePthreadAttrInit(&attr) == SCE_OK);
            while (observing.load()) {
                Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
                started.store(true);
            }
            Require(scePthreadAttrDestroy(&attr) == SCE_OK);
        });
        while (!started.load()) std::this_thread::yield();
        Require(scePthreadDetach(thread) == SCE_OK);
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
        int state = -1;
        Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK && state == DETACHED);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
        observing.store(false);
        observer.join();
        worker.release.store(true);
        while (!worker.finished.load()) std::this_thread::yield();
    }
    return 0;
}
