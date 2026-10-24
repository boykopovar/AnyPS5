#include "SceTypes.hpp"
#include "Common.hpp"
#include <atomic>
#include <new>
#include <stdexcept>
#include <thread>

struct PthreadSpinlockPrivate {
    std::atomic<std::thread::id> owner{};
};

namespace {

constexpr int GUEST_EPERM = 1;
constexpr int GUEST_ENOMEM = 12;
constexpr int GUEST_EBUSY = 16;
constexpr int PROCESS_PRIVATE = 0;
constexpr int SPIN_COUNT = 100000;

bool tryAcquire(PthreadSpinlockPrivate* spinlock) {
    std::thread::id unowned{};
    return spinlock->owner.compare_exchange_strong(unowned, std::this_thread::get_id(), std::memory_order_acquire, std::memory_order_relaxed);
}

}

extern "C" {

int APS5_VABI pthread_spin_init_nid_postfix(PthreadSpinlockPrivate** lock, int pshared) {
    if (!lock) return PosixThread::GUEST_EINVAL;
    if (pshared != PROCESS_PRIVATE) return PosixThread::GUEST_EINVAL;
    auto* spinlock = new (std::nothrow) PthreadSpinlockPrivate;
    if (!spinlock) return GUEST_ENOMEM;
    *lock = spinlock;
    return 0;
}

int APS5_VABI pthread_spin_destroy_nid_postfix(PthreadSpinlockPrivate** lock) {
    if (!lock || !*lock) return PosixThread::GUEST_EINVAL;
    delete *lock;
    *lock = nullptr;
    return 0;
}

int APS5_VABI pthread_spin_trylock_nid_postfix(PthreadSpinlockPrivate** lock) {
    if (!lock || !*lock) return PosixThread::GUEST_EINVAL;
    return tryAcquire(*lock) ? 0 : GUEST_EBUSY;
}

int APS5_VABI pthread_spin_lock_nid_postfix(PthreadSpinlockPrivate** lock) {
    if (!lock || !*lock) return PosixThread::GUEST_EINVAL;
    auto* spinlock = *lock;
    if (spinlock->owner.load(std::memory_order_relaxed) == std::this_thread::get_id())
        throw std::runtime_error("pthread_spin_lock: the calling thread already holds the lock and would spin forever");
    int count = SPIN_COUNT;
    while (!tryAcquire(spinlock)) {
        while (spinlock->owner.load(std::memory_order_relaxed) != std::thread::id{}) {
            if (--count <= 0) {
                count = SPIN_COUNT;
                std::this_thread::yield();
            }
        }
    }
    return 0;
}

int APS5_VABI pthread_spin_unlock_nid_postfix(PthreadSpinlockPrivate** lock) {
    if (!lock || !*lock) return PosixThread::GUEST_EINVAL;
    auto owner = std::this_thread::get_id();
    if (!(*lock)->owner.compare_exchange_strong(owner, std::thread::id{}, std::memory_order_release, std::memory_order_relaxed))
        return GUEST_EPERM;
    return 0;
}

}
