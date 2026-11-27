#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_SCHED_FIFO = 1;

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

static void WaitUntilReleased(Pthread thread) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int priority = 0;
    while (scePthreadGetprio(thread, &priority) != SCE_KERNEL_ERROR_ESRCH) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

int main() {
    for (int round = 0; round < 20; ++round) {
        Worker worker;
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, Wait, &worker, "released") == SCE_OK);
        while (!worker.started.load()) std::this_thread::yield();
        Require(scePthreadDetach(thread) == SCE_OK);
        std::atomic<bool> finished{false};
        std::thread observer([&] {
            while (!finished.load()) {
                int priority = 0;
                const int result = scePthreadGetprio(thread, &priority);
                Require(result == SCE_OK || result == SCE_KERNEL_ERROR_ESRCH);
                std::this_thread::yield();
            }
        });
        worker.release.store(true);
        WaitUntilReleased(thread);
        finished.store(true);
        observer.join();
        Require(scePthreadJoin(thread, nullptr) == SCE_KERNEL_ERROR_ESRCH);
        Require(scePthreadDetach(thread) == SCE_KERNEL_ERROR_ESRCH);
        Require(scePthreadCancel(thread) == SCE_KERNEL_ERROR_ESRCH);
        unsigned char name[64];
        std::memset(name, 0xAA, sizeof(name));
        Require(pthread_getname_np_nid_postfix(thread, reinterpret_cast<char*>(name)) == GUEST_ESRCH);
        Require(name[0] == 0xAA && name[sizeof(name) - 1] == 0xAA);
        int policy = -1;
        KernelSchedParam param{};
        Require(scePthreadGetschedparam(thread, &policy, &param) == SCE_KERNEL_ERROR_ESRCH);
        Require(pthread_getschedparam_nid_postfix(thread, &policy, &param) == GUEST_ESRCH);
        const KernelSchedParam requested{300};
        Require(scePthreadSetschedparam(thread, GUEST_SCHED_FIFO, &requested) == SCE_KERNEL_ERROR_ESRCH);
        Require(pthread_setschedparam_nid_postfix(thread, GUEST_SCHED_FIFO, &requested) == GUEST_ESRCH);
        int clockId = -1;
        Require(pthread_getcpuclockid_nid_postfix(thread, &clockId) == GUEST_ESRCH);
    }
    return 0;
}
