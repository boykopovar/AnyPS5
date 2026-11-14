#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int NotPermitted = static_cast<int>(0x80020001u);
constexpr int Busy = static_cast<int>(0x80020010u);
constexpr int TimedOut = static_cast<int>(0x8002003cu);
constexpr int ErrorCheck = 1;
constexpr int Recursive = 2;
constexpr int Normal = 3;
constexpr int Adaptive = 4;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

PthreadMutex MakeMutex(int type) {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0, "attribute init");
    Require(scePthreadMutexattrSettype(&attr, type) == 0, "attribute type");
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == 0, "mutex init");
    Require(scePthreadMutexattrDestroy(&attr) == 0, "attribute destroy");
    return mutex;
}

struct Task {
    void (*body)(Task*);
    PthreadMutex mutex = nullptr;
    int depth = 1;
    int iterations = 0;
    long* counter = nullptr;
    std::atomic<int>* ready = nullptr;
    std::atomic<int>* acquired = nullptr;
    std::atomic<int>* start = nullptr;
    int result = 0;
    std::int64_t elapsedMicros = 0;
    KernelUseconds timeout = 0;
    void* shared = nullptr;
};

void* APS5_VABI Entry(void* opaque) {
    auto* task = static_cast<Task*>(opaque);
    task->body(task);
    return nullptr;
}

void RunAll(std::vector<Task>& tasks) {
    std::vector<Pthread> threads(tasks.size());
    for (std::size_t index = 0; index < tasks.size(); ++index)
        Require(scePthreadCreate(&threads[index], nullptr, Entry, &tasks[index], nullptr) == 0, "thread create");
    for (auto thread : threads)
        Require(scePthreadJoin(thread, nullptr) == 0, "thread join");
}

void Increment(Task* task) {
    for (int iteration = 0; iteration < task->iterations; ++iteration) {
        for (int level = 0; level < task->depth; ++level)
            Require(scePthreadMutexLock(&task->mutex) == 0, "lock");
        const long value = *task->counter;
        *task->counter = value + 1;
        for (int level = 0; level < task->depth; ++level)
            Require(scePthreadMutexUnlock(&task->mutex) == 0, "unlock");
    }
}

void CheckExclusion(int type, int depth) {
    PthreadMutex mutex = MakeMutex(type);
    long counter = 0;
    std::vector<Task> tasks(4);
    for (auto& task : tasks) {
        task.body = Increment;
        task.mutex = mutex;
        task.depth = depth;
        task.iterations = 4000;
        task.counter = &counter;
    }
    RunAll(tasks);
    Require(counter == 4 * 4000, "increments were lost");
    Require(scePthreadMutexDestroy(&mutex) == 0, "destroy after contention");
}

void Acquire(Task* task) {
    task->ready->fetch_add(1);
    Require(scePthreadMutexLock(&task->mutex) == 0, "waiter lock");
    task->acquired->fetch_add(1);
    Require(scePthreadMutexUnlock(&task->mutex) == 0, "waiter unlock");
}

void CheckWaitersAreWoken(int type) {
    for (int round = 0; round < 50; ++round) {
        PthreadMutex mutex = MakeMutex(type);
        std::atomic<int> ready{0};
        std::atomic<int> acquired{0};
        std::vector<Task> tasks(6);
        for (auto& task : tasks) {
            task.body = Acquire;
            task.mutex = mutex;
            task.ready = &ready;
            task.acquired = &acquired;
        }
        Require(scePthreadMutexLock(&mutex) == 0, "holder lock");
        std::vector<Pthread> threads(tasks.size());
        for (std::size_t index = 0; index < tasks.size(); ++index)
            Require(scePthreadCreate(&threads[index], nullptr, Entry, &tasks[index], nullptr) == 0, "thread create");
        while (ready.load() != static_cast<int>(tasks.size())) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(round % 3));
        Require(acquired.load() == 0, "a waiter acquired a held mutex");
        Require(scePthreadMutexUnlock(&mutex) == 0, "holder unlock");
        for (auto thread : threads)
            Require(scePthreadJoin(thread, nullptr) == 0, "thread join");
        Require(acquired.load() == static_cast<int>(tasks.size()), "a waiter was not woken");
        Require(scePthreadMutexDestroy(&mutex) == 0, "destroy after handoff");
    }
}

void Probe(Task* task) {
    task->result = scePthreadMutexTrylock(&task->mutex);
    if (task->result == 0)
        Require(scePthreadMutexUnlock(&task->mutex) == 0, "probe unlock");
}

void ProbeUnlock(Task* task) {
    task->result = scePthreadMutexUnlock(&task->mutex);
}

void TimedProbe(Task* task) {
    const auto begin = std::chrono::steady_clock::now();
    task->result = scePthreadMutexTimedlock(&task->mutex, task->timeout);
    task->elapsedMicros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
    if (task->result == 0)
        Require(scePthreadMutexUnlock(&task->mutex) == 0, "timed probe unlock");
}

Task RunOne(void (*body)(Task*), PthreadMutex mutex, KernelUseconds timeout = 0) {
    std::vector<Task> tasks(1);
    tasks[0].body = body;
    tasks[0].mutex = mutex;
    tasks[0].timeout = timeout;
    RunAll(tasks);
    return tasks[0];
}

void CheckOwnership(int type) {
    PthreadMutex mutex = MakeMutex(type);
    Require(RunOne(ProbeUnlock, mutex).result == NotPermitted, "unlocking a free mutex from another thread");
    Require(scePthreadMutexLock(&mutex) == 0, "lock");
    Require(RunOne(Probe, mutex).result == Busy, "trylock of a held mutex from another thread");
    Require(RunOne(ProbeUnlock, mutex).result == NotPermitted, "unlocking a held mutex from another thread");
    const auto timed = RunOne(TimedProbe, mutex, 20000);
    Require(timed.result == TimedOut, "timedlock of a held mutex");
    Require(timed.elapsedMicros >= 15000, "timedlock returned before its timeout");
    Require(scePthreadMutexUnlock(&mutex) == 0, "unlock");
    Require(RunOne(Probe, mutex).result == 0, "trylock of a free mutex from another thread");
    Require(RunOne(TimedProbe, mutex, 20000).result == 0, "timedlock of a free mutex");
    Require(scePthreadMutexDestroy(&mutex) == 0, "destroy");
}

void CheckRecursiveDepth() {
    PthreadMutex mutex = MakeMutex(Recursive);
    constexpr int depth = 3;
    for (int level = 0; level < depth; ++level)
        Require(scePthreadMutexLock(&mutex) == 0, "recursive lock");
    Require(scePthreadMutexTrylock(&mutex) == 0, "recursive trylock by the owner");
    Require(scePthreadMutexTimedlock(&mutex, 1000) == 0, "recursive timedlock by the owner");
    for (int level = 0; level < depth + 2; ++level) {
        Require(RunOne(Probe, mutex).result == Busy, "mutex released before its last unlock");
        Require(scePthreadMutexUnlock(&mutex) == 0, "recursive unlock");
    }
    Require(RunOne(Probe, mutex).result == 0, "mutex still held after its last unlock");
    Require(scePthreadMutexUnlock(&mutex) == NotPermitted, "unlock past the lock count");
    Require(scePthreadMutexDestroy(&mutex) == 0, "destroy");
}

void CheckTimedHandoff() {
    PthreadMutex mutex = MakeMutex(ErrorCheck);
    Require(scePthreadMutexLock(&mutex) == 0, "lock");
    std::vector<Task> tasks(1);
    tasks[0].body = TimedProbe;
    tasks[0].mutex = mutex;
    tasks[0].timeout = 5000000;
    std::vector<Pthread> threads(1);
    Require(scePthreadCreate(&threads[0], nullptr, Entry, &tasks[0], nullptr) == 0, "thread create");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(scePthreadMutexUnlock(&mutex) == 0, "unlock");
    Require(scePthreadJoin(threads[0], nullptr) == 0, "join");
    Require(tasks[0].result == 0, "a timed waiter missed the release");
    Require(tasks[0].elapsedMicros < 4000000, "a timed waiter slept through the release");
    Require(scePthreadMutexDestroy(&mutex) == 0, "destroy");
}

struct Counted {
    PthreadMutex mutex = nullptr;
    std::atomic<int> start{0};
    int references = 0;
    int hold = 0;
    unsigned char padding[64];
};

void ReleaseLast(Task* task) {
    auto* counted = static_cast<Counted*>(task->shared);
    PthreadMutex* slot = &counted->mutex;
    const int hold = counted->hold;
    while (counted->start.load(std::memory_order_acquire) == 0) std::this_thread::yield();
    Require(scePthreadMutexLock(slot) == 0, "reference lock");
    for (volatile int spin = 0; spin < hold; ++spin) {}
    const bool last = --counted->references == 0;
    Require(scePthreadMutexUnlock(slot) == 0, "reference unlock");
    if (last) {
        Require(scePthreadMutexDestroy(slot) == 0, "last reference destroy");
        std::memset(counted, 0xA5, sizeof(*counted));
        delete counted;
    }
}

void CheckHandoffThenDestroy(int type) {
    for (int round = 0; round < 3000; ++round) {
        const int users = 2 + round % 3;
        auto* counted = new Counted();
        counted->mutex = MakeMutex(type);
        counted->references = users;
        counted->hold = (round % 5) * 200;
        std::vector<Task> tasks(users);
        for (auto& task : tasks) {
            task.body = ReleaseLast;
            task.shared = counted;
        }
        std::vector<Pthread> threads(tasks.size());
        for (std::size_t index = 0; index < tasks.size(); ++index)
            Require(scePthreadCreate(&threads[index], nullptr, Entry, &tasks[index], nullptr) == 0, "thread create");
        counted->start.store(1, std::memory_order_release);
        for (auto thread : threads)
            Require(scePthreadJoin(thread, nullptr) == 0, "thread join");
    }
}

}

int main() {
    for (const int type : {ErrorCheck, Normal, Adaptive})
        CheckExclusion(type, 1);
    CheckExclusion(Recursive, 1);
    CheckExclusion(Recursive, 3);
    for (const int type : {ErrorCheck, Normal, Adaptive, Recursive}) {
        CheckWaitersAreWoken(type);
        CheckOwnership(type);
    }
    for (const int type : {ErrorCheck, Normal, Adaptive, Recursive})
        CheckHandoffThenDestroy(type);
    CheckRecursiveDepth();
    CheckTimedHandoff();
}
