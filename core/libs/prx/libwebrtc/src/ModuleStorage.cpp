#include "prx/libwebrtc/include/ModuleStorage.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <new>

namespace {

constexpr std::size_t BigAllocationBytes = 0x1000;
constexpr std::size_t BigAllocationPadding = 0x27;
constexpr std::uintptr_t BigAllocationAlignment = 0x20;

}

namespace WebRtcModuleStorage {

void* Allocate(std::size_t bytes) {
    const bool big = bytes >= BigAllocationBytes;
    void* raw = malloc_nid_postfix(big ? bytes + BigAllocationPadding : bytes);
    if (!raw)
        throw std::bad_alloc();
    if (!big)
        return raw;
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + BigAllocationPadding) & ~(BigAllocationAlignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

void Free(void* storage, std::size_t capacityBytes) {
    if (!storage)
        return;
    if (capacityBytes >= BigAllocationBytes)
        storage = static_cast<void**>(storage)[-1];
    free_nid_postfix(storage);
}

}
