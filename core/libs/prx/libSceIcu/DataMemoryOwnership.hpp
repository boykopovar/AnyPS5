#pragma once

#include <mutex>
#include <unordered_map>

namespace IcuDataMemoryOwnership {

struct TrustedRecord {
    const void* pHeader;
    void* mapAddr;
    void* map;
};

inline std::unordered_map<void*, TrustedRecord> registry;
inline std::mutex registryMutex;

inline void Register(void* pointer, const void* pHeader, void* mapAddr, void* map) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    registry.emplace(pointer, TrustedRecord{pHeader, mapAddr, map});
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
