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
    *__error_nid_postfix() = 42;
    _ZNSt3__15mutex4lockEv_nid_postfix(&mutex);
    Require(mutex != nullptr, "libc++ mutex lock did not initialize the static pthread handle");
    Require(*__error_nid_postfix() == 42, "libc++ mutex lock changed guest errno");

    std::atomic<bool> otherAcquired{true};
    std::thread contender([&] {
        otherAcquired.store(_ZNSt3__15mutex8try_lockEv_nid_postfix(&mutex));
        if (otherAcquired.load()) _ZNSt3__15mutex6unlockEv_nid_postfix(&mutex);
    });
    contender.join();
    Require(!otherAcquired.load(), "libc++ mutex try_lock accepted another thread while held");

    std::atomic<bool> nonOwnerUnlockThrew{false};
    std::thread nonOwner([&] {
        try {
            _ZNSt3__15mutex6unlockEv_nid_postfix(&mutex);
        } catch (...) {
            nonOwnerUnlockThrew.store(true);
        }
    });
    nonOwner.join();
    Require(!nonOwnerUnlockThrew.load(), "libc++ mutex non-owner unlock threw");
    Require(mutex != nullptr, "libc++ mutex non-owner unlock changed the handle");
    bool heldDestroyThrew = false;
    try {
        _ZNSt3__15mutexD1Ev_nid_postfix(&mutex);
    } catch (...) {
        heldDestroyThrew = true;
    }
    Require(!heldDestroyThrew && mutex != nullptr, "libc++ mutex destruction while held did not ignore the pthread error");

    std::atomic<bool> attempted{false};
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        attempted.store(true);
        _ZNSt3__15mutex4lockEv_nid_postfix(&mutex);
        acquired.store(true);
        _ZNSt3__15mutex6unlockEv_nid_postfix(&mutex);
    });
    while (!attempted.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const bool blocked = !acquired.load();
    _ZNSt3__15mutex6unlockEv_nid_postfix(&mutex);
    waiter.join();
    Require(blocked && acquired.load(), "libc++ mutex did not block then wake a waiter");

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "guest pthread could not lock native handle");
    std::thread other([&] { otherAcquired.store(_ZNSt3__15mutex8try_lockEv_nid_postfix(&mutex)); });
    other.join();
    Require(!otherAcquired.load(), "libc++ mutex did not share ownership with guest pthread");
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "guest pthread could not unlock native handle");
    Require(_ZNSt3__15mutex8try_lockEv_nid_postfix(&mutex), "libc++ mutex try_lock failed after unlock");
    _ZNSt3__15mutex6unlockEv_nid_postfix(&mutex);
    _ZNSt3__15mutexD1Ev_nid_postfix(&mutex);

    bool rejected = false;
    try {
        _ZNSt3__15mutex4lockEv_nid_postfix(reinterpret_cast<PthreadMutex*>(std::uintptr_t{1}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "libc++ mutex accepted an unaligned guest address");
    return 0;
}
