#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr std::uint32_t SemaFifo = 1;
static constexpr std::uint32_t SemaPriority = 2;

static void Require(bool value) { if (!value) std::abort(); }

struct Waiter {
    KernelSema sem = nullptr;
    int result = -1;
};

static void* APS5_VABI Wait(void* arg) {
    auto& waiter = *static_cast<Waiter*>(arg);
    KernelUseconds timeout = 1000000;
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
    KernelUseconds timeout = 5000000;
};

static void* APS5_VABI WaitInQueue(void* arg) {
    auto& waiter = *static_cast<QueuedWaiter*>(arg);
    if (waiter.priority != 0) Require(scePthreadSetprio(scePthreadSelf(), waiter.priority) == SCE_OK);
    KernelUseconds timeout = waiter.timeout;
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

static void CheckStopsAtALargerNeed(std::uint32_t attr) {
    released = 0;
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "handoff-head", attr, 0, 4, nullptr) == SCE_OK);
    QueuedWaiter large{sem, 2};
    QueuedWaiter small{sem, 1};
    const Pthread largeThread = StartQueued(large, 1);
    const Pthread smallThread = StartQueued(small, 2);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(released.load() == 0 && QueuedCount(sem) == 2);
    Require(sceKernelPollSema(sem, 1) == SCE_OK && sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelSignalSema(sem, 2) == SCE_OK);
    WaitForReleased(1);
    Require(large.order == 1 && large.result == SCE_OK && small.order == 0);
    Require(QueuedCount(sem) == 1);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    WaitForReleased(2);
    Require(small.order == 2 && small.result == SCE_OK);
    Require(scePthreadJoin(largeThread, nullptr) == SCE_OK && scePthreadJoin(smallThread, nullptr) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}

static void CheckTimedOutHeadStaysBlocking(std::uint32_t attr) {
    released = 0;
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "handoff-timed-out", attr, 0, 4, nullptr) == SCE_OK);
    QueuedWaiter head{sem, 3};
    head.timeout = 400000;
    QueuedWaiter next{sem, 1};
    next.timeout = 1000000;
    const Pthread headThread = StartQueued(head, 1);
    const Pthread nextThread = StartQueued(next, 2);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(released.load() == 0 && QueuedCount(sem) == 2);
    WaitForReleased(1);
    Require(head.order == 1 && head.result == SCE_KERNEL_ERROR_ETIMEDOUT && QueuedCount(sem) == 1);
    WaitForReleased(2);
    Require(next.order == 2 && next.result == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadJoin(headThread, nullptr) == SCE_OK && scePthreadJoin(nextThread, nullptr) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_OK && sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}

static void CheckDeletedRefusesFreeTokens() {
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "handoff-deleted", SemaFifo, 2, 2, nullptr) == SCE_OK);
    {
        std::lock_guard<std::mutex> lock(sem->mutex);
        sem->deleted = true;
    }
    Require(sceKernelWaitSema(sem, 1, nullptr) == SCE_KERNEL_ERROR_EACCES);
    {
        std::lock_guard<std::mutex> lock(sem->mutex);
        Require(sem->tokenCount == 2);
        sem->deleted = false;
    }
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
        WaitForQueue(sem, 1);
        Require(sceKernelSignalSema(sem, 1) == SCE_OK);
        Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
        Require(waiter.result == SCE_OK);
        Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);
        Require(sceKernelDeleteSema(sem) == SCE_OK);
    }
    CheckStopsAtALargerNeed(SemaFifo);
    CheckStopsAtALargerNeed(SemaPriority);
    CheckStopsAtALargerNeed(0);
    CheckTimedOutHeadStaysBlocking(SemaFifo);
    CheckTimedOutHeadStaysBlocking(SemaPriority);
    CheckTimedOutHeadStaysBlocking(0);
    CheckDeletedRefusesFreeTokens();
    CheckServesInOrder(SemaFifo, 700, 700, true);
    CheckServesInOrder(SemaFifo, 767, 256, true);
    CheckServesInOrder(SemaPriority, 767, 256, false);
    CheckServesInOrder(SemaPriority, 256, 767, true);
    CheckServesInOrder(SemaPriority, 700, 700, true);
    return 0;
}
