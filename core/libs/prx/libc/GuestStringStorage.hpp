#pragma once

#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

namespace GuestStringStorage {

constexpr std::size_t LargeAllocation = 0x1000;
constexpr std::size_t AllocationPadding = 0x27;
constexpr std::uintptr_t AllocationAlignment = 0x20;

static inline void* Allocate(std::size_t bytes) {
    if (bytes < LargeAllocation) return _Znwm_nid_postfix(bytes);
    if (bytes > std::numeric_limits<std::size_t>::max() - AllocationPadding) throw std::bad_alloc();
    auto* raw = static_cast<unsigned char*>(_Znwm_nid_postfix(bytes + AllocationPadding));
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + AllocationPadding) & ~(AllocationAlignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

static inline void Deallocate(void* storage, std::size_t bytes) {
    if (bytes >= LargeAllocation) storage = static_cast<void**>(storage)[-1];
    _ZdlPv_nid_postfix(storage);
}

}
