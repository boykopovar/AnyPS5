#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI sceUltFinalize();
int APS5_VABI sceUltMutexCreate(void*, const char*, void*, const UltMutexOptParam*, std::uint32_t);
int APS5_VABI sceUltMutexLock(void*);
int APS5_VABI sceUltMutexUnlock(void*);
int APS5_VABI sceUltMutexDestroy(void*);
int APS5_VABI sceUltConditionVariableCreate(void*, const char*, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltConditionVariableCreate(void*, const char*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltConditionVariableWait(void*);
int APS5_VABI sceUltConditionVariableSignal(void*);
int APS5_VABI sceUltConditionVariableSignalAll(void*);
int APS5_VABI sceUltConditionVariableDestroy(void*);
}

constexpr int Null = -2139029503;
constexpr int Invalid = -2139029500;
constexpr int Permission = -2139029499;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;

static void Require(bool ok, int line) {
    if (!ok) {
        std::fprintf(stderr, "ULT condition check failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(value) Require((value), __LINE__)

struct alignas(8) Object { std::array<std::uint8_t, 256> bytes{}; };

struct Fixture {
    Object mutex;
    Object condition;

    explicit Fixture(bool recursive = false) {
        UltMutexOptParam options{};
        options.attribute = recursive ? 1 : 0;
        CHECK(sceUltMutexCreate(&mutex, "mutex", nullptr, &options, 0x12000000) == 0);
        CHECK(_sceUltConditionVariableCreate(&condition, "condition", &mutex, nullptr, 0x12000000) == 0);
    }

    void Lock() { CHECK(sceUltMutexLock(&mutex) == 0); }
    void Unlock() { CHECK(sceUltMutexUnlock(&mutex) == 0); }
    void Wait() { CHECK(sceUltConditionVariableWait(&condition) == 0); }
    void Signal() { CHECK(sceUltConditionVariableSignal(&condition) == 0); }

    ~Fixture() {
        CHECK(sceUltConditionVariableDestroy(&condition) == 0);
        CHECK(sceUltMutexDestroy(&mutex) == 0);
    }
};

static void Validation() {
    Fixture f(true);
    Object unused;
    CHECK(sceUltMutexDestroy(nullptr) == Null);
    CHECK(sceUltMutexDestroy(&unused) == State);
    CHECK(sceUltConditionVariableCreate(nullptr, nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltConditionVariableCreate(&unused, nullptr, &unused, nullptr, 0) == Invalid);
    CHECK(sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == State);
    CHECK(_sceUltConditionVariableCreate(nullptr, nullptr, nullptr, nullptr, 0) == Null);
    CHECK(_sceUltConditionVariableCreate(&unused, nullptr, &unused, nullptr, 0) == Invalid);
    CHECK(_sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == State);
    CHECK(sceUltConditionVariableDestroy(nullptr) == Null);
    CHECK(sceUltConditionVariableDestroy(&unused) == State);
    CHECK(sceUltConditionVariableSignal(nullptr) == Null);
    CHECK(sceUltConditionVariableSignal(&unused) == State);
    CHECK(sceUltConditionVariableSignalAll(nullptr) == Null);
    CHECK(sceUltConditionVariableSignalAll(&unused) == State);
    CHECK(sceUltConditionVariableSignalAll(&f.condition) == 0);
    CHECK(sceUltConditionVariableWait(nullptr) == Null);
    CHECK(sceUltConditionVariableWait(&unused) == State);
    CHECK(_sceUltConditionVariableCreate(&unused, nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltConditionVariableWait(&f.condition) == State);
    CHECK(sceUltMutexUnlock(&f.mutex) == Permission);
    bool unsupported = false;
    try { _sceUltConditionVariableCreate(&unused, nullptr, &f.mutex, &unused, 0); }
    catch (const std::runtime_error&) { unsupported = true; }
    CHECK(unsupported);
    f.Lock();
    CHECK(sceUltMutexDestroy(&f.mutex) == State);
    CHECK(sceUltMutexCreate(&f.mutex, nullptr, nullptr, nullptr, 0) == State);
    auto other = std::async(std::launch::async, [&] {
        CHECK(sceUltMutexUnlock(&f.mutex) == Permission);
        CHECK(sceUltConditionVariableWait(&f.condition) == State);
    });
    other.get();
    f.Lock();
    unsupported = false;
    try { f.Wait(); } catch (const std::runtime_error&) { unsupported = true; }
    CHECK(unsupported);
    f.Unlock();
    CHECK(sceUltMutexDestroy(&f.mutex) == State);
    f.Unlock();
    CHECK(sceUltMutexDestroy(&f.mutex) == State);
    CHECK(sceUltConditionVariableDestroy(&f.condition) == 0);
    CHECK(sceUltMutexDestroy(&f.mutex) == 0);
    CHECK(sceUltMutexLock(&f.mutex) == State);
    CHECK(sceUltMutexCreate(&f.mutex, nullptr, nullptr, nullptr, 0) == 0);
    CHECK(sceUltConditionVariableDestroy(&f.condition) == State);
    CHECK(sceUltConditionVariableSignal(&f.condition) == State);
    CHECK(_sceUltConditionVariableCreate(&f.condition, nullptr, &f.mutex, nullptr, 0) == 0);
}

static void Await(const std::atomic<int>& count, int expected) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (count.load() != expected) {
        CHECK(std::chrono::steady_clock::now() < end);
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
        CHECK(payload == 42);
        CHECK(sceUltMutexDestroy(&f.mutex) == State);
        f.Unlock();
    });
    Await(entered, 1);
    f.Lock();
    CHECK(sceUltConditionVariableDestroy(&f.condition) == Busy);
    CHECK(sceUltMutexDestroy(&f.mutex) == State);
    CHECK(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    f.Unlock();
    f.Lock();
    payload = 42;
    f.Signal();
    CHECK(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    CHECK(sceUltConditionVariableDestroy(&f.condition) == Busy);
    f.Unlock();
    CHECK(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
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
    CHECK(finished == 1);
    f.Lock();
    f.Signal();
    f.Signal();
    f.Unlock();
    for (auto& waiter : waiters) {
        CHECK(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
        waiter.get();
    }
    CHECK(finished == 3);
}

static void SignalAll() {
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
    CHECK(sceUltConditionVariableSignalAll(&f.condition) == 0);
    CHECK(sceUltConditionVariableDestroy(&f.condition) == Busy);
    f.Unlock();
    for (auto& waiter : waiters) {
        CHECK(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
        waiter.get();
    }
    CHECK(finished == 3);
    std::atomic<int> late = 0;
    auto after = std::async(std::launch::async, [&] {
        f.Lock();
        late = 1;
        f.Wait();
        f.Unlock();
    });
    Await(late, 1);
    f.Lock();
    f.Unlock();
    CHECK(after.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    f.Lock();
    f.Signal();
    f.Unlock();
    CHECK(after.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    after.get();
}

static void Stress() {
    Fixture f;
    Object consumed;
    CHECK(_sceUltConditionVariableCreate(&consumed, nullptr, &f.mutex, nullptr, 0) == 0);
    int payload = 0;
    auto consumer = std::async(std::launch::async, [&] {
        for (int expected = 1; expected <= 5000; ++expected) {
            f.Lock();
            while (payload == 0) f.Wait();
            CHECK(payload == expected);
            payload = 0;
            CHECK(sceUltConditionVariableSignal(&consumed) == 0);
            f.Unlock();
        }
    });
    for (int value = 1; value <= 5000; ++value) {
        f.Lock();
        while (payload != 0) CHECK(sceUltConditionVariableWait(&consumed) == 0);
        payload = value;
        f.Signal();
        f.Unlock();
    }
    CHECK(consumer.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    consumer.get();
    CHECK(sceUltConditionVariableDestroy(&consumed) == 0);
}

static void Finalize(bool signaled) {
    Object mutex, condition;
    CHECK(sceUltMutexCreate(&mutex, "mutex", nullptr, nullptr, 0) == 0);
    CHECK(_sceUltConditionVariableCreate(&condition, "condition", &mutex, nullptr, 0) == 0);
    std::atomic<int> entered = 0;
    auto waiter = std::async(std::launch::async, [&] {
        CHECK(sceUltMutexLock(&mutex) == 0);
        entered = 1;
        CHECK(sceUltConditionVariableWait(&condition) == State);
        CHECK(sceUltMutexUnlock(&mutex) == State);
    });
    Await(entered, 1);
    CHECK(sceUltMutexLock(&mutex) == 0);
    if (signaled) CHECK(sceUltConditionVariableSignal(&condition) == 0);
    CHECK(waiter.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    CHECK(sceUltConditionVariableDestroy(&condition) == Busy);
    CHECK(sceUltFinalize() == 0);
    CHECK(waiter.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    waiter.get();
    CHECK(sceUltConditionVariableWait(&condition) == State);
    CHECK(sceUltMutexLock(&mutex) == State);
    CHECK(sceUltMutexCreate(&mutex, "reopened", nullptr, nullptr, 0) == 0);
    CHECK(_sceUltConditionVariableCreate(&condition, "reopened", &mutex, nullptr, 0) == 0);
    CHECK(sceUltConditionVariableDestroy(&condition) == 0);
    CHECK(sceUltMutexDestroy(&mutex) == 0);
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
    CHECK(sceUltFinalize() == 0);
    std::puts("ULT condition tests passed");
}
