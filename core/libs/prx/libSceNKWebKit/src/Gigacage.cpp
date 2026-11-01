#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
void* TryAllocate(std::size_t size, std::size_t alignment = 16);
void* Reallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void* TryReallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16);
void Deallocate(void* ptr);
}  // namespace NKWebKit

extern "C" {
void* APS5_VABI _ZN8Gigacage6mallocENS_4KindEm(std::uint32_t, std::size_t size) {
    return NKWebKit::Allocate(size);
}

void* APS5_VABI _ZN8Gigacage9tryMallocENS_4KindEm(std::uint32_t, std::size_t size) {
    return NKWebKit::TryAllocate(size);
}

void APS5_VABI _ZN8Gigacage4freeENS_4KindEPv(std::uint32_t, void* ptr) {
    NKWebKit::Deallocate(ptr);
}

void* APS5_VABI _ZN8Gigacage11mallocArrayENS_4KindEmm(std::uint32_t, std::size_t count, std::size_t size) {
    if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
        std::abort();
    }
    return NKWebKit::Allocate(count * size);
}

void* APS5_VABI _ZN8Gigacage14tryMallocArrayENS_4KindEmm(std::uint32_t, std::size_t count, std::size_t size) {
    if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
        return nullptr;
    }
    return NKWebKit::TryAllocate(count * size);
}

void* APS5_VABI _ZN8Gigacage16tryAlignedMallocENS_4KindEmm(std::uint32_t, std::size_t alignment, std::size_t size) {
    return NKWebKit::TryAllocate(size, alignment);
}

void APS5_VABI _ZN8Gigacage11alignedFreeENS_4KindEPv(std::uint32_t, void* ptr) {
    NKWebKit::Deallocate(ptr);
}

void* APS5_VABI _ZN8Gigacage10tryReallocENS_4KindEPvm(std::uint32_t, void* ptr, std::size_t size) {
    return NKWebKit::TryReallocate(ptr, size);
}

void* APS5_VABI _ZN8Gigacage29tryAllocateZeroedVirtualPagesENS_4KindEm(std::uint32_t, std::size_t size) {
    void* ptr = NKWebKit::TryAllocate(size, 4096);
    if (ptr) {
        std::memset(ptr, 0, size);
    }
    return ptr;
}

void APS5_VABI _ZN8Gigacage16freeVirtualPagesENS_4KindEPvm(std::uint32_t, void* ptr, std::size_t) {
    NKWebKit::Deallocate(ptr);
}

}
