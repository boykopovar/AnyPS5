#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace IcuDataMemoryOwnership {

struct TrustedRecord {
    const void* pHeader;
    void* mapAddr;
    void* map;
};

inline std::mutex registryMutex;
inline std::unordered_map<void*, TrustedRecord> registry;
inline std::unordered_set<void*> freed;

inline void MarkFreed(void* pointer) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    freed.insert(pointer);
}

inline bool IsFreed(void* pointer) {
    if (!pointer) return false;
    std::lock_guard<std::mutex> lock(registryMutex);
    return freed.find(pointer) != freed.end();
}

inline void Register(void* pointer, const void* pHeader, void* mapAddr, void* map) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    freed.erase(pointer);
    registry[pointer] = TrustedRecord{pHeader, mapAddr, map};
}

inline bool TryTake(void* pointer, TrustedRecord& out) {
    if (!pointer) return false;
    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = registry.find(pointer);
    if (it == registry.end()) return false;
    out = it->second;
    registry.erase(it);
    return true;
}

}
