#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/LibcxxPlainMutexForwards.hpp"
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

template <typename Fn>
bool ThrowsRuntimeError(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct PlainMutexForwardsGuard {
    static inline const LibcxxPlainMutexForwards_nid_no_patch validForwards{
        pthread_mutex_destroy_nid_postfix,
        pthread_mutex_lock_nid_postfix,
        pthread_mutex_trylock_nid_postfix,
        pthread_mutex_unlock_nid_postfix,
    };

    PlainMutexForwardsGuard() = default;
    ~PlainMutexForwardsGuard() { RegisterLibcxxPlainMutexForwards_nid_no_patch(&validForwards); }
    PlainMutexForwardsGuard(const PlainMutexForwardsGuard&) = delete;
    PlainMutexForwardsGuard& operator=(const PlainMutexForwardsGuard&) = delete;
};

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

    auto throwsInvalidArgument = [](auto&& fn) {
        try {
            fn();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    Require(throwsInvalidArgument([&] {
                _ZNSt3__15mutex4lockEv_nid_postfix(reinterpret_cast<PthreadMutex*>(std::uintptr_t{1}));
            }),
            "libc++ mutex accepted an unaligned guest address");
    for (PthreadMutex* badAddress : {
             static_cast<PthreadMutex*>(nullptr),
             reinterpret_cast<PthreadMutex*>(std::uintptr_t{0x10}),
         }) {
        Require(throwsInvalidArgument([&] { _ZNSt3__15mutex4lockEv_nid_postfix(badAddress); }),
                "libc++ mutex lock accepted an unwritable guest address");
        Require(throwsInvalidArgument([&] { static_cast<void>(_ZNSt3__15mutex8try_lockEv_nid_postfix(badAddress)); }),
                "libc++ mutex try_lock accepted an unwritable guest address");
        Require(throwsInvalidArgument([&] { _ZNSt3__15mutex6unlockEv_nid_postfix(badAddress); }),
                "libc++ mutex unlock accepted an unwritable guest address");
        Require(throwsInvalidArgument([&] { _ZNSt3__15mutexD1Ev_nid_postfix(badAddress); }),
                "libc++ mutex destructor accepted an unwritable guest address");
    }

    {
        static const LibcxxPlainMutexForwards_nid_no_patch partialForwards{
            pthread_mutex_destroy_nid_postfix,
            nullptr,
            nullptr,
            pthread_mutex_unlock_nid_postfix,
        };
        PlainMutexForwardsGuard guard;
        RegisterLibcxxPlainMutexForwards_nid_no_patch(&partialForwards);

        PthreadMutex partialMutex = nullptr;
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutex4lockEv_nid_postfix(&partialMutex); }),
                "libc++ mutex lock accepted a partial forward table");
        Require(ThrowsRuntimeError([&] { static_cast<void>(_ZNSt3__15mutex8try_lockEv_nid_postfix(&partialMutex)); }),
                "libc++ mutex try_lock accepted a partial forward table");
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutex6unlockEv_nid_postfix(&partialMutex); }),
                "libc++ mutex unlock accepted a partial forward table");
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutexD1Ev_nid_postfix(&partialMutex); }),
                "libc++ mutex destructor accepted a partial forward table");
    }

    {
        PlainMutexForwardsGuard guard;
        LibcxxPlainMutexForwards_nid_no_patch stackForwards = PlainMutexForwardsGuard::validForwards;
        RegisterLibcxxPlainMutexForwards_nid_no_patch(&stackForwards);
        stackForwards = {};

        PthreadMutex copiedMutex = nullptr;
        _ZNSt3__15mutex4lockEv_nid_postfix(&copiedMutex);
        Require(copiedMutex != nullptr, "libc++ mutex lock failed after caller forward table was zeroed");
        _ZNSt3__15mutex6unlockEv_nid_postfix(&copiedMutex);
        Require(_ZNSt3__15mutex8try_lockEv_nid_postfix(&copiedMutex),
                "libc++ mutex try_lock failed after caller forward table was zeroed");
        _ZNSt3__15mutex6unlockEv_nid_postfix(&copiedMutex);
        _ZNSt3__15mutexD1Ev_nid_postfix(&copiedMutex);

        RegisterLibcxxPlainMutexForwards_nid_no_patch(nullptr);
        PthreadMutex clearedMutex = nullptr;
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutex4lockEv_nid_postfix(&clearedMutex); }),
                "libc++ mutex lock accepted a null forward table");
        Require(ThrowsRuntimeError([&] { static_cast<void>(_ZNSt3__15mutex8try_lockEv_nid_postfix(&clearedMutex)); }),
                "libc++ mutex try_lock accepted a null forward table");
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutex6unlockEv_nid_postfix(&clearedMutex); }),
                "libc++ mutex unlock accepted a null forward table");
        Require(ThrowsRuntimeError([&] { _ZNSt3__15mutexD1Ev_nid_postfix(&clearedMutex); }),
                "libc++ mutex destructor accepted a null forward table");
    }

    PthreadMutex restoredMutex = nullptr;
    _ZNSt3__15mutex4lockEv_nid_postfix(&restoredMutex);
    Require(restoredMutex != nullptr, "libc++ mutex lock failed after forward restoration");
    _ZNSt3__15mutex6unlockEv_nid_postfix(&restoredMutex);
    Require(_ZNSt3__15mutex8try_lockEv_nid_postfix(&restoredMutex),
            "libc++ mutex try_lock failed after forward restoration");
    _ZNSt3__15mutex6unlockEv_nid_postfix(&restoredMutex);
    _ZNSt3__15mutexD1Ev_nid_postfix(&restoredMutex);
    return 0;
}
