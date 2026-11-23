#include "prx/libSceIcu/Mutex.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

std::mutex TableLock;

std::mutex& GlobalMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<const UMutex*, std::unique_ptr<std::mutex>>& Mutexes() {
    static std::unordered_map<const UMutex*, std::unique_ptr<std::mutex>> mutexes;
    return mutexes;
}

std::mutex& Created(const UMutex* mutex) {
    if (!mutex) return GlobalMutex();
    const std::lock_guard lock(TableLock);
    auto& entry = Mutexes()[mutex];
    if (!entry) entry = std::make_unique<std::mutex>();
    return *entry;
}

std::mutex InitLock;
std::condition_variable& InitDone() {
    static auto* const done = new std::condition_variable();
    return *done;
}

std::atomic_ref<std::int32_t> InitState(Icu::InitOnce* once, const char* caller) {
    if (!once) throw std::invalid_argument(std::string(caller) + ": null UInitOnce");
    return std::atomic_ref<std::int32_t>(once->state);
}

std::mutex& Existing(const UMutex* mutex) {
    if (!mutex) return GlobalMutex();
    const std::lock_guard lock(TableLock);
    const auto found = Mutexes().find(mutex);
    if (found == Mutexes().end()) throw std::invalid_argument("umtx_unlock: mutex was never locked");
    return *found->second;
}

}

extern "C" {

void APS5_VABI umtx_lock_nid_postfix(UMutex* mutex) {
    Created(mutex).lock();
}

void APS5_VABI umtx_unlock_nid_postfix(UMutex* mutex) {
    Existing(mutex).unlock();
}

std::int8_t APS5_VABI _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once) {
    const auto state = InitState(once, "umtx_initImplPreInit");
    std::unique_lock lock(InitLock);
    if (state.load(std::memory_order_acquire) == 0) {
        state.store(1, std::memory_order_release);
        return 1;
    }
    InitDone().wait(lock, [&] { return state.load(std::memory_order_acquire) != 1; });
    return 0;
}

void APS5_VABI _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once) {
    const auto state = InitState(once, "umtx_initImplPostInit");
    {
        const std::lock_guard lock(InitLock);
        state.store(2, std::memory_order_release);
    }
    InitDone().notify_all();
}

}
