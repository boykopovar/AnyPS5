#include "prx/libSceIcu/libSceIcu.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "umtx check failed: %s\n", what);
    std::abort();
}

struct Storage {
    alignas(8) unsigned char bytes[24]{};
    UMutex* get() { return reinterpret_cast<UMutex*>(bytes); }
};

void Global() {
    umtx_lock_nid_postfix(nullptr);
    umtx_unlock_nid_postfix(nullptr);
    Storage other;
    umtx_lock_nid_postfix(nullptr);
    umtx_lock_nid_postfix(other.get());
    umtx_unlock_nid_postfix(other.get());
    umtx_unlock_nid_postfix(nullptr);
}

void Exclusion() {
    Storage storage;
    long counter = 0;
    auto work = [&] {
        for (int i = 0; i < 100000; ++i) {
            umtx_lock_nid_postfix(storage.get());
            const long value = counter;
            counter = value + 1;
            umtx_unlock_nid_postfix(storage.get());
        }
    };
    std::thread first(work);
    std::thread second(work);
    first.join();
    second.join();
    Require(counter == 200000, "two threads lost an update");
}

void Blocking() {
    Storage storage;
    std::atomic<bool> acquired = false;
    umtx_lock_nid_postfix(storage.get());
    std::thread waiter([&] {
        umtx_lock_nid_postfix(storage.get());
        acquired = true;
        umtx_unlock_nid_postfix(storage.get());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!acquired, "a held mutex was acquired by another thread");
    umtx_unlock_nid_postfix(storage.get());
    waiter.join();
    Require(acquired, "the waiter never acquired the released mutex");
}

void UntouchedStorage() {
    Storage storage;
    umtx_lock_nid_postfix(storage.get());
    umtx_unlock_nid_postfix(storage.get());
    for (unsigned char byte : storage.bytes) Require(byte == 0, "the guest mutex storage was written");
}

void UnlockWithoutLock() {
    static Storage storage;
    try {
        umtx_unlock_nid_postfix(storage.get());
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "unlocking a never locked mutex did not throw");
}

std::int8_t PreInit(Icu::InitOnce& once) {
    return _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(&once);
}

void PostInit(Icu::InitOnce& once) {
    _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(&once);
}

std::int32_t State(Icu::InitOnce& once) {
    return std::atomic_ref<std::int32_t>(once.state).load();
}

void InitOnceStates() {
    for (const std::int32_t state : {0, 2, 3, -1, INT_MAX, INT_MIN, 0x100, 0x10002}) {
        for (const std::int32_t error : {0, 5, -128}) {
            Icu::InitOnce block[3] = {{0x11111111, 0x22222222}, {state, error}, {0x33333333, 0x44444444}};
            const std::int8_t first = PreInit(block[1]);
            Require(first == (state == 0 ? 1 : 0), "umtx_initImplPreInit returned the wrong value");
            Require(block[1].state == (state == 0 ? 1 : state), "umtx_initImplPreInit left the wrong state");
            PostInit(block[1]);
            Require(block[1].state == 2, "umtx_initImplPostInit did not mark the object done");
            Require(PreInit(block[1]) == 0 && block[1].state == 2, "umtx_initImplPreInit ran a finished initialization");
            Require(block[1].errorCode == error, "the error code was written");
            Require(block[0].state == 0x11111111 && block[0].errorCode == 0x22222222, "the previous object was written");
            Require(block[2].state == 0x33333333 && block[2].errorCode == 0x44444444, "the next object was written");
        }
    }
}

void InitOnceWaits() {
    Icu::InitOnce once{0, 0};
    Require(PreInit(once) == 1, "the first caller was not chosen");
    constexpr int Waiters = 8;
    std::atomic<int> started = 0;
    std::atomic<int> finished = 0;
    std::vector<std::int8_t> results(Waiters, -1);
    std::vector<std::thread> waiters;
    for (int index = 0; index != Waiters; ++index) {
        waiters.emplace_back([&, index] {
            ++started;
            results[index] = PreInit(once);
            ++finished;
        });
    }
    while (started != Waiters) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(finished == 0, "a caller returned while the initialization was running");
    PostInit(once);
    for (auto& waiter : waiters) waiter.join();
    for (const auto result : results) Require(result == 0, "a waiting caller ran the initialization");
    Require(State(once) == 2, "the object is not done after the waiters");
}

void InitOnceWakesOnAnyPostInit() {
    Icu::InitOnce once{1, 0};
    std::atomic<bool> finished = false;
    std::int8_t result = -1;
    std::thread waiter([&] {
        result = PreInit(once);
        finished = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!finished, "a caller returned while the state was 1");
    std::atomic_ref<std::int32_t>(once.state).store(5);
    Icu::InitOnce other{0, 0};
    Require(PreInit(other) == 1, "another object was not free");
    PostInit(other);
    waiter.join();
    Require(result == 0 && State(once) == 5, "the waiter did not leave on a state other than 1");
}

void InitOnceRace() {
    constexpr int Objects = 10000;
    for (const int threads : {2, 8, 32, 64}) {
        std::vector<Icu::InitOnce> objects(Objects, Icu::InitOnce{0, 0});
        std::vector<std::atomic<int>> runs(Objects);
        std::atomic<bool> start = false;
        std::atomic<int> failures = 0;
        std::vector<std::thread> pool;
        for (int thread = 0; thread != threads; ++thread) {
            pool.emplace_back([&, thread] {
                while (!start) std::this_thread::yield();
                for (int step = 0; step != Objects; ++step) {
                    auto& object = objects[(step * 7 + thread * 131) % Objects];
                    auto& count = runs[(step * 7 + thread * 131) % Objects];
                    if (State(object) == 2) continue;
                    if (PreInit(object)) {
                        if (++count != 1) ++failures;
                        PostInit(object);
                    } else if (State(object) != 2 || count != 1) {
                        ++failures;
                    }
                }
            });
        }
        start = true;
        for (auto& thread : pool) thread.join();
        Require(failures == 0, "a caller saw an unfinished or repeated initialization");
        for (const auto& count : runs) Require(count == 1, "an object was not initialized exactly once");
    }
}

void InitOnceNull() {
    bool preThrew = false;
    bool postThrew = false;
    try {
        _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        preThrew = true;
    }
    try {
        _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        postThrew = true;
    }
    Require(preThrew && postThrew, "a null UInitOnce did not throw");
}

void InitOnceWaiterAtExit() {
    static Icu::InitOnce once{1, 0};
    std::thread([] { PreInit(once); }).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}

int main() {
    Global();
    Exclusion();
    Blocking();
    UntouchedStorage();
    UnlockWithoutLock();
    InitOnceStates();
    InitOnceWaits();
    InitOnceWakesOnAnyPostInit();
    InitOnceRace();
    InitOnceNull();
    InitOnceWaiterAtExit();
    return 0;
}
