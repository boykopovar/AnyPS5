#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
}

static constexpr int SCE_OK = 0;
static constexpr std::uint32_t SemaFifo = 1;
static constexpr std::uint32_t SemaPriority = 2;

static void Require(bool value) { if (!value) std::abort(); }

struct Waiter {
    KernelSema sem = nullptr;
    int result = -1;
    std::atomic<bool> started{false};
};

static void* APS5_VABI Wait(void* arg) {
    auto& waiter = *static_cast<Waiter*>(arg);
    KernelUseconds timeout = 1000000;
    waiter.started = true;
    waiter.result = sceKernelWaitSema(waiter.sem, 1, &timeout);
    return nullptr;
}

static std::atomic<int> released{0};

static int QueuedCount(KernelSema sem) {
    std::lock_guard<std::mutex> lock(sem->mutex);
    return static_cast<int>(sem->queue.size());
}

struct QueuedWaiter {
    KernelSema sem = nullptr;
    int need = 1;
    int priority = 0;
    int result = -1;
    std::atomic<int> order{0};
};

static void* APS5_VABI WaitInQueue(void* arg) {
    auto& waiter = *static_cast<QueuedWaiter*>(arg);
    if (waiter.priority != 0) Require(scePthreadSetprio(scePthreadSelf(), waiter.priority) == SCE_OK);
    KernelUseconds timeout = 5000000;
    waiter.result = sceKernelWaitSema(waiter.sem, waiter.need, &timeout);
    waiter.order = ++released;
    return nullptr;
}

static void WaitForQueue(KernelSema sem, int count) {
    for (int i = 0; i < 5000 && QueuedCount(sem) != count; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(QueuedCount(sem) == count);
}

static void WaitForReleased(int count) {
    for (int i = 0; i < 5000 && released.load() < count; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(released.load() == count);
}

static Pthread StartQueued(QueuedWaiter& waiter, int queued) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, WaitInQueue, &waiter, nullptr) == SCE_OK);
    WaitForQueue(waiter.sem, queued);
    return thread;
}

static void CheckPassesOverALargerNeed() {
    released = 0;
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "handoff-skip", SemaFifo, 0, 4, nullptr) == SCE_OK);
    QueuedWaiter large{sem, 2};
    QueuedWaiter small{sem, 1};
    const Pthread largeThread = StartQueued(large, 1);
    const Pthread smallThread = StartQueued(small, 2);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    WaitForReleased(1);
    Require(small.order == 1 && small.result == SCE_OK);
    Require(QueuedCount(sem) == 1 && sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelSignalSema(sem, 2) == SCE_OK);
    WaitForReleased(2);
    Require(large.order == 2 && large.result == SCE_OK);
    Require(scePthreadJoin(largeThread, nullptr) == SCE_OK && scePthreadJoin(smallThread, nullptr) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}

static void CheckServesInOrder(std::uint32_t attr, int firstPriority, int secondPriority, bool firstServedFirst) {
    released = 0;
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "handoff-order", attr, 0, 2, nullptr) == SCE_OK);
    QueuedWaiter first{sem, 1, firstPriority};
    QueuedWaiter second{sem, 1, secondPriority};
    const Pthread firstThread = StartQueued(first, 1);
    const Pthread secondThread = StartQueued(second, 2);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    WaitForReleased(1);
    Require((firstServedFirst ? first.order : second.order) == 1);
    Require(QueuedCount(sem) == 1);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    WaitForReleased(2);
    Require(first.result == SCE_OK && second.result == SCE_OK);
    Require(scePthreadJoin(firstThread, nullptr) == SCE_OK && scePthreadJoin(secondThread, nullptr) == SCE_OK);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}

int main() {
    for (int i = 0; i < 20; ++i) {
        KernelSema sem = nullptr;
        Require(sceKernelCreateSema(&sem, "handoff", 1, 0, 1, nullptr) == SCE_OK);
        Waiter waiter{sem};
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, Wait, &waiter, nullptr) == SCE_OK);
        while (!waiter.started) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(sceKernelSignalSema(sem, 1) == SCE_OK);
        Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
        Require(waiter.result == SCE_OK);
        Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
        Require(sceKernelDeleteSema(sem) == SCE_OK);
    }
    CheckPassesOverALargerNeed();
    CheckServesInOrder(SemaFifo, 700, 700, true);
    CheckServesInOrder(SemaFifo, 767, 256, true);
    CheckServesInOrder(SemaPriority, 767, 256, false);
    CheckServesInOrder(SemaPriority, 256, 767, true);
    CheckServesInOrder(SemaPriority, 700, 700, true);
    return 0;
}
