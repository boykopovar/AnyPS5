#pragma once

#include <mutex>
#include <unordered_set>

namespace IcuDataMemoryOwnership {

inline std::unordered_set<void*> registry;
inline std::mutex registryMutex;

inline void Register(void* pointer) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    registry.insert(pointer);
}

inline bool TryTake(void* pointer) {
    if (!pointer) return false;
    std::lock_guard<std::mutex> lock(registryMutex);
    return registry.erase(pointer) > 0;
}

}
