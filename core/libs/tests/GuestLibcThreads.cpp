#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

extern "C" int APS5_VABI _Mtx_init_nid_postfix(void**, int);
extern "C" void APS5_VABI _Mtx_destroy_nid_postfix(void**);
extern "C" int APS5_VABI _Mtx_lock_nid_postfix(void**);
extern "C" int APS5_VABI _Mtx_unlock_nid_postfix(void**);
extern "C" int APS5_VABI _Mtx_trylock_nid_postfix(void**);
extern "C" int APS5_VABI _Cnd_init_nid_postfix(void**);
extern "C" void APS5_VABI _Cnd_destroy_nid_postfix(void**);
extern "C" int APS5_VABI _Cnd_wait_nid_postfix(void**, void**);
extern "C" int APS5_VABI _Cnd_broadcast_nid_postfix(void**);
extern "C" int APS5_VABI _Cnd_signal_nid_postfix(void**);

static void Require(bool value) { if (!value) std::abort(); }

static void PlainTypes() {
    for (int type : {1, 2, 3, 5}) {
        void* mutex = nullptr;
        Require(_Mtx_init_nid_postfix(&mutex, type) == 0 && mutex != nullptr);
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        Require(_Mtx_lock_nid_postfix(&mutex) == 3);
        Require(_Mtx_trylock_nid_postfix(&mutex) == 3);
        int fromOther = -1, tryOther = -1;
        std::thread([&] { fromOther = _Mtx_unlock_nid_postfix(&mutex); tryOther = _Mtx_trylock_nid_postfix(&mutex); }).join();
        Require(fromOther == 4 && tryOther == 3);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 4);
        Require(_Mtx_trylock_nid_postfix(&mutex) == 0);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
        _Mtx_destroy_nid_postfix(&mutex);
        Require(mutex == nullptr);
    }
}

static void Recursive() {
    void* mutex = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, 0x101) == 0);
    for (int i = 0; i < 2; ++i) Require(_Mtx_lock_nid_postfix(&mutex) == 0);
    Require(_Mtx_trylock_nid_postfix(&mutex) == 0);
    std::atomic<bool> taken{false};
    std::thread other([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        taken = true;
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    });
    for (int i = 0; i < 2; ++i) Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!taken);
    Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    other.join();
    Require(taken);
    Require(_Mtx_unlock_nid_postfix(&mutex) == 4);
    _Mtx_destroy_nid_postfix(&mutex);
}

static void Contention() {
    void* mutex = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, 1) == 0);
    long counter = 0;
    std::array<std::thread, 8> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        for (int i = 0; i < 2000; ++i) {
            Require(_Mtx_lock_nid_postfix(&mutex) == 0);
            ++counter;
            Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
        }
    });
    for (auto& worker : workers) worker.join();
    Require(counter == 16000);
    _Mtx_destroy_nid_postfix(&mutex);
}

static void ZeroInitialized() {
    void* mutex = nullptr;
    void* condition = nullptr;
    Require(_Mtx_lock_nid_postfix(&mutex) == 0 && mutex != nullptr);
    Require(_Mtx_lock_nid_postfix(&mutex) == 3);
    Require(_Cnd_broadcast_nid_postfix(&condition) == 0 && condition != nullptr);
    Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
    Require(condition == nullptr && mutex == nullptr);
}

static void Broadcast() {
    void* mutex = nullptr;
    void* condition = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, 1) == 0);
    Require(_Cnd_init_nid_postfix(&condition) == 0 && condition != nullptr);
    Require(_Cnd_wait_nid_postfix(&condition, &mutex) == 4);
    bool ready = false;
    int waiting = 0, woken = 0;
    std::array<std::thread, 4> waiters;
    for (auto& waiter : waiters) waiter = std::thread([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        ++waiting;
        while (!ready) Require(_Cnd_wait_nid_postfix(&condition, &mutex) == 0);
        ++woken;
        Require(_Mtx_lock_nid_postfix(&mutex) == 3);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    });
    for (;;) {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        const bool all = waiting == 4;
        if (all) {
            ready = true;
            Require(_Cnd_broadcast_nid_postfix(&condition) == 0);
        }
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
        if (all) break;
        std::this_thread::yield();
    }
    for (auto& waiter : waiters) waiter.join();
    Require(woken == 4);
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
}

static void Signal() {
    void* mutex = nullptr;
    void* condition = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, 1) == 0);
    Require(_Cnd_init_nid_postfix(&condition) == 0);
    int tokens = 0, consumed = 0;
    std::array<std::thread, 3> consumers;
    for (auto& consumer : consumers) consumer = std::thread([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        while (tokens == 0) Require(_Cnd_wait_nid_postfix(&condition, &mutex) == 0);
        --tokens;
        ++consumed;
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    });
    for (int i = 0; i < 3; ++i) {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        ++tokens;
        Require(_Cnd_signal_nid_postfix(&condition) == 0);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    }
    for (auto& consumer : consumers) consumer.join();
    Require(consumed == 3 && tokens == 0);
    Require(_Cnd_signal_nid_postfix(nullptr) == 4);
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
}

static void RecursiveWait() {
    void* mutex = nullptr;
    void* condition = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, 0x100) == 0);
    Require(_Cnd_init_nid_postfix(&condition) == 0);
    bool ready = false;
    Require(_Mtx_lock_nid_postfix(&mutex) == 0);
    Require(_Mtx_lock_nid_postfix(&mutex) == 0);
    std::thread signaller([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == 0);
        ready = true;
        Require(_Cnd_broadcast_nid_postfix(&condition) == 0);
        Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    });
    while (!ready) Require(_Cnd_wait_nid_postfix(&condition, &mutex) == 0);
    signaller.join();
    Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    Require(_Mtx_unlock_nid_postfix(&mutex) == 0);
    Require(_Mtx_unlock_nid_postfix(&mutex) == 4);
    _Cnd_destroy_nid_postfix(&condition);
    _Mtx_destroy_nid_postfix(&mutex);
}

int main() {
    Require(_Mtx_init_nid_postfix(nullptr, 1) == 4);
    Require(_Mtx_lock_nid_postfix(nullptr) == 4);
    Require(_Cnd_init_nid_postfix(nullptr) == 4);
    _Mtx_destroy_nid_postfix(nullptr);
    _Cnd_destroy_nid_postfix(nullptr);
    PlainTypes();
    Recursive();
    Contention();
    ZeroInitialized();
    Broadcast();
    Signal();
    RecursiveWait();
    return 0;
}
