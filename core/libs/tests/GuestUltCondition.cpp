#include "prx/libSceUlt/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

constexpr int Null = -2139029503;
constexpr int Alignment = -2139029502;
constexpr int Invalid = -2139029500;
constexpr int Permission = -2139029499;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;

struct alignas(8) Object { std::array<std::uint8_t, 256> bytes{}; };

struct Fixture {
    Object mutex;
    Object condition;

    explicit Fixture(bool recursive = false) {
        UltMutexOptParam options{};
        options.attribute = recursive ? 1 : 0;
        Require(sceUltMutexCreate(&mutex, "mutex", nullptr, &options, 0x12000000) == 0);
        Require(_sceUltConditionVariableCreate(&condition, "condition", &mutex, nullptr, 0x12000000) == 0);
    }

    void Lock() { Require(sceUltMutexLock(&mutex) == 0); }
    void Unlock() { Require(sceUltMutexUnlock(&mutex) == 0); }
    void Wait() { Require(sceUltConditionVariableWait(&condition) == 0); }
    void Signal() { Require(sceUltConditionVariableSignal(&condition) == 0); }

    ~Fixture() {
        Require(sceUltConditionVariableDestroy(&condition) == 0);
        Require(sceUltMutexDestroy(&mutex) == 0);
    }
};

static void Validation() {
    Fixture f(true);
    Object unused;
    Require(sceUltMutexDestroy(nullptr) == Null);
    Require(sceUltMutexDestroy(&unused) == State);
    Require(sceUltConditionVariableCreate(nullptr, nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltConditionVariableCreate(&unused, nullptr, &unused, nullptr, 0) == Invalid);
    Require(sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == State);
    Require(_sceUltConditionVariableCreate(nullptr, nullptr, nullptr, nullptr, 0) == Null);
    Require(_sceUltConditionVariableCreate(&unused, nullptr, &unused, nullptr, 0) == Invalid);
    Require(_sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == State);
    Require(sceUltConditionVariableDestroy(nullptr) == Null);
    Require(sceUltConditionVariableDestroy(&unused) == State);
    Require(sceUltConditionVariableSignal(nullptr) == Null);
    Require(sceUltConditionVariableSignal(&unused) == State);
    Require(sceUltConditionVariableSignal(reinterpret_cast<unsigned char*>(&unused) + 1) == Alignment);
    Require(sceUltConditionVariableSignal(reinterpret_cast<unsigned char*>(&unused) + 4) == Alignment);
    Require(sceUltConditionVariableWait(nullptr) == Null);
    Require(sceUltConditionVariableWait(&unused) == State);
    Require(_sceUltConditionVariableCreate(&unused, nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltConditionVariableWait(&f.condition) == State);
    Require(sceUltMutexUnlock(&f.mutex) == Permission);
    bool unsupported = false;
    try { _sceUltConditionVariableCreate(&unused, nullptr, &f.mutex, &unused, 0); }
    catch (const std::runtime_error&) { unsupported = true; }
    Require(unsupported);
    f.Lock();
    Require(sceUltMutexDestroy(&f.mutex) == State);
    Require(sceUltMutexCreate(&f.mutex, nullptr, nullptr, nullptr, 0) == State);
    auto other = std::async(std::launch::async, [&] {
        Require(sceUltMutexUnlock(&f.mutex) == Permission);
        Require(sceUltConditionVariableWait(&f.condition) == State);
    });
    other.get();
    f.Lock();
    unsupported = false;
    try { f.Wait(); } catch (const std::runtime_error&) { unsupported = true; }
    Require(unsupported);
    f.Unlock();
    Require(sceUltMutexDestroy(&f.mutex) == State);
    f.Unlock();
    Require(sceUltMutexDestroy(&f.mutex) == State);
    Require(sceUltConditionVariableDestroy(&f.condition) == 0);
    Require(sceUltMutexDestroy(&f.mutex) == 0);
    Require(sceUltMutexLock(&f.mutex) == State);
    Require(sceUltMutexCreate(&f.mutex, nullptr, nullptr, nullptr, 0) == 0);
    Require(sceUltConditionVariableDestroy(&f.condition) == State);
    Require(sceUltConditionVariableSignal(&f.condition) == State);
    Require(_sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == 0);
}

static void Await(const std::atomic<int>& count, int expected) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (count.load() != expected) {
        Require(std::chrono::steady_clock::now() < end);
        std::this_thread::yield();
    }
}

static void Handoff() {
    Fixture f;
    f.Signal();
    std::atomic<int> entered = 0;
    int payload = 0;
    auto waiter = std::async(std::launch::async, [&] {
        f.Lock();
        entered = 1;
        f.Wait();
        Require(payload == 42);
        Require(sceUltMutexDestroy(&f.mutex) == State);
        f.Unlock();
    });
    Await(entered, 1);
    f.Lock();
    Require(sceUltConditionVariableDestroy(&f.condition) == Busy);
    Require(sceUltMutexDestroy(&f.mutex) == State);
    Require(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    f.Unlock();
    f.Lock();
    payload = 42;
    f.Signal();
    Require(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    Require(sceUltConditionVariableDestroy(&f.condition) == Busy);
    f.Unlock();
    Require(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    waiter.get();
}

static void SignalOne() {
    Fixture f;
    std::atomic<int> entered = 0, finished = 0;
    std::array<std::future<void>, 3> waiters;
    for (auto& waiter : waiters) waiter = std::async(std::launch::async, [&] {
        f.Lock();
        ++entered;
        f.Wait();
        ++finished;
        f.Unlock();
    });
    Await(entered, 3);
    f.Lock();
    f.Signal();
    f.Unlock();
    Await(finished, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(finished == 1);
    f.Lock();
    f.Signal();
    f.Signal();
    f.Unlock();
    for (auto& waiter : waiters) {
        Require(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
        waiter.get();
    }
    Require(finished == 3);
}

static void SignalAll() {
    Object unused;
    Require(sceUltConditionVariableSignalAll(nullptr) == Null);
    Require(sceUltConditionVariableSignalAll(&unused) == State);
    Fixture f;
    Require(sceUltConditionVariableSignalAll(&f.condition) == 0);
    std::atomic<int> entered = 0, holding = 0, finished = 0;
    const auto waiter = [&] {
        f.Lock();
        ++entered;
        f.Wait();
        Require(++holding == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        --holding;
        ++finished;
        f.Unlock();
    };
    std::array<std::future<void>, 4> waiters;
    waiters[0] = std::async(std::launch::async, waiter);
    Await(entered, 1);
    f.Lock();
    f.Unlock();
    Require(waiters[0].wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    waiters[1] = std::async(std::launch::async, waiter);
    waiters[2] = std::async(std::launch::async, waiter);
    Await(entered, 3);
    f.Lock();
    Require(sceUltConditionVariableSignalAll(&f.condition) == 0);
    Require(waiters[0].wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    Require(finished == 0);
    f.Unlock();
    Await(finished, 3);
    waiters[3] = std::async(std::launch::async, waiter);
    Await(entered, 4);
    f.Lock();
    f.Unlock();
    Require(waiters[3].wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    f.Lock();
    f.Signal();
    f.Unlock();
    for (auto& future : waiters) {
        Require(future.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
        future.get();
    }
    Require(finished == 4);
    Require(sceUltConditionVariableDestroy(&f.condition) == 0);
    Require(sceUltConditionVariableSignalAll(&f.condition) == State);
    Require(_sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == 0);
}

static void Stress() {
    Fixture f;
    Object consumed;
    Require(_sceUltConditionVariableCreate(&consumed, nullptr, &f.mutex, nullptr, 0) == 0);
    int payload = 0;
    auto consumer = std::async(std::launch::async, [&] {
        for (int expected = 1; expected <= 5000; ++expected) {
            f.Lock();
            while (payload == 0) f.Wait();
            Require(payload == expected);
            payload = 0;
            Require(sceUltConditionVariableSignal(&consumed) == 0);
            f.Unlock();
        }
    });
    for (int value = 1; value <= 5000; ++value) {
        f.Lock();
        while (payload != 0) Require(sceUltConditionVariableWait(&consumed) == 0);
        payload = value;
        f.Signal();
        f.Unlock();
    }
    Require(consumer.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    consumer.get();
    Require(sceUltConditionVariableDestroy(&consumed) == 0);
}

static void Finalize(bool signaled) {
    Object mutex, condition;
    Require(sceUltMutexCreate(&mutex, "mutex", nullptr, nullptr, 0) == 0);
    Require(_sceUltConditionVariableCreate(&condition, "condition", &mutex, nullptr, 0) == 0);
    std::atomic<int> entered = 0;
    auto waiter = std::async(std::launch::async, [&] {
        Require(sceUltMutexLock(&mutex) == 0);
        entered = 1;
        Require(sceUltConditionVariableWait(&condition) == State);
        Require(sceUltMutexUnlock(&mutex) == State);
    });
    Await(entered, 1);
    Require(sceUltMutexLock(&mutex) == 0);
    if (signaled) Require(sceUltConditionVariableSignal(&condition) == 0);
    Require(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    Require(sceUltConditionVariableDestroy(&condition) == Busy);
    Require(sceUltFinalize() == 0);
    Require(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    waiter.get();
    Require(sceUltConditionVariableWait(&condition) == State);
    Require(sceUltMutexLock(&mutex) == State);
    Require(sceUltMutexCreate(&mutex, "reopened", nullptr, nullptr, 0) == 0);
    Require(_sceUltConditionVariableCreate(&condition, "reopened", &mutex, nullptr, 0) == 0);
    Require(sceUltConditionVariableDestroy(&condition) == 0);
    Require(sceUltMutexDestroy(&mutex) == 0);
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const std::string test = argv[1];
    if (test == "validation") Validation();
    else if (test == "handoff") Handoff();
    else if (test == "signal-one") SignalOne();
    else if (test == "signal-all") SignalAll();
    else if (test == "stress") Stress();
    else if (test == "finalize-queued") Finalize(false);
    else if (test == "finalize-reacquire") Finalize(true);
    else return 1;
    Require(sceUltFinalize() == 0);
    std::puts("ULT condition tests passed");
}
