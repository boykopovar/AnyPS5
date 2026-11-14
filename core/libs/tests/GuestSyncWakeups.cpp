#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

constexpr int Rounds = 5000;
constexpr KernelUseconds Patience = 5000000;
constexpr uint32_t WaitOr = 0x02;
constexpr uint32_t ClearPattern = 0x20;
constexpr uint32_t MultipleWaiters = 0x20;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct Task {
    void (*body)(Task*) = nullptr;
    KernelSema first = nullptr;
    KernelSema second = nullptr;
    KernelEventFlag flag = nullptr;
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    int* turn = nullptr;
    int self = 0;
    std::atomic<int>* waiting = nullptr;
    std::atomic<int>* released = nullptr;
};

void* APS5_VABI Entry(void* opaque) {
    auto* task = static_cast<Task*>(opaque);
    task->body(task);
    return nullptr;
}

Pthread Start(Task& task) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Entry, &task, nullptr) == 0, "thread create");
    return thread;
}

void Join(Pthread thread) {
    Require(scePthreadJoin(thread, nullptr) == 0, "thread join");
}

void SemaphoreEcho(Task* task) {
    for (int round = 0; round < Rounds; ++round) {
        KernelUseconds patience = Patience;
        Require(sceKernelWaitSema(task->first, 1, &patience) == 0, "echo missed a semaphore signal");
        Require(sceKernelSignalSema(task->second, 1) == 0, "echo signal");
    }
}

void CheckSemaphores() {
    Task task;
    task.body = SemaphoreEcho;
    Require(sceKernelCreateSema(&task.first, "request", 0, 0, 1, nullptr) == 0, "create request semaphore");
    Require(sceKernelCreateSema(&task.second, "reply", 0, 0, 1, nullptr) == 0, "create reply semaphore");
    const Pthread thread = Start(task);
    for (int round = 0; round < Rounds; ++round) {
        Require(sceKernelSignalSema(task.first, 1) == 0, "request signal");
        KernelUseconds patience = Patience;
        Require(sceKernelWaitSema(task.second, 1, &patience) == 0, "a semaphore reply was lost");
    }
    Join(thread);
    Require(sceKernelDeleteSema(task.first) == 0, "delete request semaphore");
    Require(sceKernelDeleteSema(task.second) == 0, "delete reply semaphore");
}

void EventFlagEcho(Task* task) {
    for (int round = 0; round < Rounds; ++round) {
        KernelUseconds patience = Patience;
        uint64_t result = 0;
        Require(sceKernelWaitEventFlag(task->flag, 1, WaitOr | ClearPattern, &result, &patience) == 0, "echo missed an event flag");
        Require(sceKernelSetEventFlag(task->flag, 2) == 0, "echo set");
    }
}

void CheckEventFlags() {
    Task task;
    task.body = EventFlagEcho;
    Require(sceKernelCreateEventFlag(&task.flag, "echo", MultipleWaiters, 0, nullptr) == 0, "create event flag");
    const Pthread thread = Start(task);
    for (int round = 0; round < Rounds; ++round) {
        Require(sceKernelSetEventFlag(task.flag, 1) == 0, "request set");
        KernelUseconds patience = Patience;
        uint64_t result = 0;
        Require(sceKernelWaitEventFlag(task.flag, 2, WaitOr | ClearPattern, &result, &patience) == 0, "an event flag reply was lost");
        Require(result == 2, "unexpected event flag pattern");
    }
    Join(thread);
    Require(sceKernelDeleteEventFlag(task.flag) == 0, "delete event flag");
}

void ConditionEcho(Task* task) {
    for (int round = 0; round < Rounds; ++round) {
        Require(scePthreadMutexLock(&task->mutex) == 0, "echo lock");
        while (*task->turn != 1)
            Require(scePthreadCondTimedwait(&task->cond, &task->mutex, Patience) == 0, "echo missed a condition signal");
        *task->turn = 0;
        Require(scePthreadCondSignal(&task->cond) == 0, "echo signal");
        Require(scePthreadMutexUnlock(&task->mutex) == 0, "echo unlock");
    }
}

void CheckCondition() {
    Task task;
    int turn = 0;
    task.body = ConditionEcho;
    task.turn = &turn;
    Require(scePthreadMutexInit(&task.mutex, nullptr, nullptr) == 0, "mutex init");
    Require(scePthreadCondInit(&task.cond, nullptr, nullptr) == 0, "cond init");
    const Pthread thread = Start(task);
    for (int round = 0; round < Rounds; ++round) {
        Require(scePthreadMutexLock(&task.mutex) == 0, "lock");
        turn = 1;
        Require(scePthreadCondSignal(&task.cond) == 0, "signal");
        while (turn != 0)
            Require(scePthreadCondTimedwait(&task.cond, &task.mutex, Patience) == 0, "a condition reply was lost");
        Require(scePthreadMutexUnlock(&task.mutex) == 0, "unlock");
    }
    Join(thread);
    Require(scePthreadCondDestroy(&task.cond) == 0, "cond destroy");
    Require(scePthreadMutexDestroy(&task.mutex) == 0, "mutex destroy");
}

void BroadcastWaiter(Task* task) {
    Require(scePthreadMutexLock(&task->mutex) == 0, "waiter lock");
    task->waiting->fetch_add(1);
    while (*task->turn == 0)
        Require(scePthreadCondTimedwait(&task->cond, &task->mutex, Patience) == 0, "a waiter missed the broadcast");
    task->released->fetch_add(1);
    Require(scePthreadMutexUnlock(&task->mutex) == 0, "waiter unlock");
}

void CheckBroadcast() {
    for (int round = 0; round < 40; ++round) {
        PthreadMutex mutex = nullptr;
        PthreadCond cond = nullptr;
        Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0, "mutex init");
        Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0, "cond init");
        int turn = 0;
        std::atomic<int> waiting{0};
        std::atomic<int> released{0};
        std::vector<Task> tasks(6);
        std::vector<Pthread> threads;
        for (auto& task : tasks) {
            task.body = BroadcastWaiter;
            task.mutex = mutex;
            task.cond = cond;
            task.turn = &turn;
            task.waiting = &waiting;
            task.released = &released;
            threads.push_back(Start(task));
        }
        while (waiting.load() != static_cast<int>(tasks.size()))
            std::this_thread::yield();
        Require(scePthreadMutexLock(&mutex) == 0, "lock");
        turn = 1;
        Require(scePthreadMutexUnlock(&mutex) == 0, "unlock");
        const auto begin = std::chrono::steady_clock::now();
        Require(scePthreadCondBroadcast(&cond) == 0, "broadcast");
        for (auto thread : threads)
            Join(thread);
        Require(std::chrono::steady_clock::now() - begin < std::chrono::seconds(4), "a broadcast waited for a timeout");
        Require(released.load() == static_cast<int>(tasks.size()), "a waiter was not released");
        Require(scePthreadCondDestroy(&cond) == 0, "cond destroy");
        Require(scePthreadMutexDestroy(&mutex) == 0, "mutex destroy");
    }
}

}

int main() {
    CheckSemaphores();
    CheckEventFlags();
    CheckCondition();
    CheckBroadcast();
}
