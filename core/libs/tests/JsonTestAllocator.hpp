#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace JsonTest {

inline void* APS5_VABI Allocate(void*, std::size_t bytes, void*) {
    return std::malloc(bytes);
}

inline void APS5_VABI Free(void*, void* pointer, void*) {
    std::free(pointer);
}

inline void APS5_VABI Notify(void*, int error, std::size_t bytes, void*) {
    std::fprintf(stderr, "Unexpected Json2 allocation error 0x%x for %zu bytes\n", error, bytes);
    std::abort();
}

inline void* const AllocatorVtable[] = {
    nullptr, nullptr, reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Notify)
};

struct Allocator {
    void* const* vtable = AllocatorVtable;
};

}
