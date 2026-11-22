#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    static_assert(sizeof(PthreadMutex) == sizeof(std::uint64_t));
    PthreadMutex mutex = nullptr;
    _ZNSt3__115recursive_mutexC1Ev_nid_postfix(&mutex);
    Require(mutex != nullptr, "libc++ recursive mutex constructor did not create a pthread mutex");

    *__error_nid_postfix() = 42;
    _ZNSt3__115recursive_mutex4lockEv_nid_postfix(&mutex);
    _ZNSt3__115recursive_mutex4lockEv_nid_postfix(&mutex);
    Require(_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(&mutex), "libc++ recursive mutex rejected a third lock by its owner");
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    Require(*__error_nid_postfix() == 42, "libc++ recursive mutex changed guest errno on success");

    _ZNSt3__115recursive_mutex4lockEv_nid_postfix(&mutex);
    std::atomic<bool> otherAcquired{true};
    std::thread contender([&] {
        otherAcquired.store(_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(&mutex));
        if (otherAcquired.load()) _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    });
    contender.join();
    Require(!otherAcquired.load(), "libc++ recursive mutex try_lock accepted a different thread");

    std::atomic<bool> nonOwnerUnlockThrew{false};
    std::thread nonOwner([&] {
        try {
            _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
        } catch (...) {
            nonOwnerUnlockThrew.store(true);
        }
    });
    nonOwner.join();
    Require(!nonOwnerUnlockThrew.load(), "libc++ recursive mutex non-owner unlock threw");
    bool heldDestroyThrew = false;
    try {
        _ZNSt3__115recursive_mutexD1Ev_nid_postfix(&mutex);
    } catch (...) {
        heldDestroyThrew = true;
    }
    Require(!heldDestroyThrew && mutex != nullptr, "libc++ recursive mutex destruction while held did not ignore the pthread error");

    std::atomic<bool> attempted{false};
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        attempted.store(true);
        _ZNSt3__115recursive_mutex4lockEv_nid_postfix(&mutex);
        acquired.store(true);
        _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    });
    while (!attempted.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const bool blocked = !acquired.load();
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    waiter.join();
    Require(blocked && acquired.load(), "libc++ recursive mutex did not block then wake a second thread");

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "guest pthread could not lock the libc++ native handle");
    Require(_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(&mutex), "libc++ recursive mutex could not relock its pthread native handle");
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "guest pthread could not unlock the libc++ native handle");

    _ZNSt3__115recursive_mutexD1Ev_nid_postfix(&mutex);
    bool invalidRejected = false;
    try {
        _ZNSt3__115recursive_mutexC1Ev_nid_postfix(reinterpret_cast<PthreadMutex*>(std::uintptr_t{1}));
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    Require(invalidRejected, "libc++ recursive mutex accepted an unaligned guest address");
    return 0;
}
