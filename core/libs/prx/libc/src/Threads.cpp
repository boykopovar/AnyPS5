#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>

namespace {

constexpr int ThrdSuccess = 0;
constexpr int ThrdNomem = 1;
constexpr int ThrdBusy = 3;
constexpr int ThrdError = 4;
constexpr int MtxRecursive = 0x100;

struct LibcMutex {
    std::mutex state;
    std::condition_variable released;
    std::thread::id owner;
    unsigned long long count = 0;
    bool recursive = false;
};

struct LibcCondition {
    std::condition_variable_any waiters;
};

template<typename T>
T* Resolve(T** slot) {
    std::atomic_ref<T*> reference(*slot);
    if (T* current = reference.load(std::memory_order_acquire)) return current;
    T* created = new (std::nothrow) T();
    if (created == nullptr) return nullptr;
    T* expected = nullptr;
    if (reference.compare_exchange_strong(expected, created, std::memory_order_acq_rel)) return created;
    delete created;
    return expected;
}

template<typename T>
void Destroy(T** slot) {
    if (slot == nullptr) return;
    delete std::atomic_ref<T*>(*slot).exchange(nullptr, std::memory_order_acq_rel);
}

void Acquire(LibcMutex* mutex, std::unique_lock<std::mutex>& lock, unsigned long long count) {
    mutex->released.wait(lock, [mutex] { return mutex->owner == std::thread::id{}; });
    mutex->owner = std::this_thread::get_id();
    mutex->count = count;
}

void Release(LibcMutex* mutex) {
    mutex->owner = std::thread::id{};
    mutex->count = 0;
    mutex->released.notify_one();
}

}

extern "C" {

int APS5_VABI _Mtx_init_nid_postfix(LibcMutex** slot, int type) {
    if (slot == nullptr) return ThrdError;
    LibcMutex* created = new (std::nothrow) LibcMutex();
    if (created == nullptr) return ThrdNomem;
    created->recursive = (type & MtxRecursive) != 0;
    std::atomic_ref<LibcMutex*>(*slot).store(created, std::memory_order_release);
    return ThrdSuccess;
}

void APS5_VABI _Mtx_destroy_nid_postfix(LibcMutex** slot) {
    Destroy(slot);
}

int APS5_VABI _Mtx_lock_nid_postfix(LibcMutex** slot) {
    if (slot == nullptr) return ThrdError;
    LibcMutex* mutex = Resolve(slot);
    if (mutex == nullptr) return ThrdError;
    std::unique_lock lock(mutex->state);
    if (mutex->owner == std::this_thread::get_id()) {
        if (!mutex->recursive) return ThrdBusy;
        ++mutex->count;
        return ThrdSuccess;
    }
    Acquire(mutex, lock, 1);
    return ThrdSuccess;
}

int APS5_VABI _Mtx_trylock_nid_postfix(LibcMutex** slot) {
    if (slot == nullptr) return ThrdError;
    LibcMutex* mutex = Resolve(slot);
    if (mutex == nullptr) return ThrdError;
    std::lock_guard lock(mutex->state);
    if (mutex->owner == std::this_thread::get_id()) {
        if (!mutex->recursive) return ThrdBusy;
        ++mutex->count;
        return ThrdSuccess;
    }
    if (mutex->owner != std::thread::id{}) return ThrdBusy;
    mutex->owner = std::this_thread::get_id();
    mutex->count = 1;
    return ThrdSuccess;
}

int APS5_VABI _Mtx_unlock_nid_postfix(LibcMutex** slot) {
    if (slot == nullptr) return ThrdError;
    LibcMutex* mutex = std::atomic_ref<LibcMutex*>(*slot).load(std::memory_order_acquire);
    if (mutex == nullptr) return ThrdError;
    std::lock_guard lock(mutex->state);
    if (mutex->owner != std::this_thread::get_id()) return ThrdError;
    if (--mutex->count == 0) Release(mutex);
    return ThrdSuccess;
}

int APS5_VABI _Cnd_init_nid_postfix(LibcCondition** slot) {
    if (slot == nullptr) return ThrdError;
    LibcCondition* created = new (std::nothrow) LibcCondition();
    if (created == nullptr) return ThrdNomem;
    std::atomic_ref<LibcCondition*>(*slot).store(created, std::memory_order_release);
    return ThrdSuccess;
}

void APS5_VABI _Cnd_destroy_nid_postfix(LibcCondition** slot) {
    Destroy(slot);
}

int APS5_VABI _Cnd_wait_nid_postfix(LibcCondition** conditionSlot, LibcMutex** mutexSlot) {
    if (conditionSlot == nullptr || mutexSlot == nullptr) return ThrdError;
    LibcCondition* condition = Resolve(conditionSlot);
    LibcMutex* mutex = std::atomic_ref<LibcMutex*>(*mutexSlot).load(std::memory_order_acquire);
    if (condition == nullptr || mutex == nullptr) return ThrdError;
    std::unique_lock lock(mutex->state);
    if (mutex->owner != std::this_thread::get_id()) return ThrdError;
    const unsigned long long count = mutex->count;
    Release(mutex);
    condition->waiters.wait(lock);
    Acquire(mutex, lock, count);
    return ThrdSuccess;
}

int APS5_VABI _Cnd_signal_nid_postfix(LibcCondition** slot) {
    if (slot == nullptr) return ThrdError;
    LibcCondition* condition = Resolve(slot);
    if (condition == nullptr) return ThrdError;
    condition->waiters.notify_one();
    return ThrdSuccess;
}

int APS5_VABI _Cnd_broadcast_nid_postfix(LibcCondition** slot) {
    if (slot == nullptr) return ThrdError;
    LibcCondition* condition = Resolve(slot);
    if (condition == nullptr) return ThrdError;
    condition->waiters.notify_all();
    return ThrdSuccess;
}

}
