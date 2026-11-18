#include "prx/libSceIcu/Mutex.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>
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

}
