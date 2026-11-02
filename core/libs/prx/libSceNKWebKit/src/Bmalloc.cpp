#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>

namespace NKWebKit {

struct AllocationHeader {
    void* raw;
    std::size_t size;
};

void* PlaceAllocation(std::size_t size, std::size_t alignment) {
    if (alignment < alignof(std::max_align_t)) {
        alignment = alignof(std::max_align_t);
    }
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr;
    }
    constexpr std::size_t overhead = sizeof(AllocationHeader);
    if (alignment > std::numeric_limits<std::size_t>::max() - overhead) {
        return nullptr;
    }
    const std::size_t slack = alignment + overhead;
    if (size > std::numeric_limits<std::size_t>::max() - slack) {
        return nullptr;
    }
    void* raw = std::malloc(size + slack);
    if (!raw) {
        return nullptr;
    }
    const std::uintptr_t rawAddr = reinterpret_cast<std::uintptr_t>(raw);
    const std::uintptr_t payloadAddr = (rawAddr + overhead + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    auto* header = reinterpret_cast<AllocationHeader*>(payloadAddr - overhead);
    header->raw = raw;
    header->size = size;
    return reinterpret_cast<void*>(payloadAddr);
}

void* Allocate(std::size_t size, std::size_t alignment = 16) {
    void* ptr = PlaceAllocation(size, alignment);
    if (!ptr) {
        std::abort();
    }
    return ptr;
}

void* TryAllocate(std::size_t size, std::size_t alignment = 16) {
    return PlaceAllocation(size, alignment);
}

void Deallocate(void* ptr) {
    if (!ptr) {
        return;
    }
    auto* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<char*>(ptr) - sizeof(AllocationHeader));
    std::free(header->raw);
}

std::size_t AllocationSize(const void* ptr) {
    if (!ptr) {
        return 0;
    }
    const auto* header = reinterpret_cast<const AllocationHeader*>(reinterpret_cast<const char*>(ptr) - sizeof(AllocationHeader));
    return header->size;
}

void* Reallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16) {
    if (!ptr) {
        return Allocate(newSize, alignment);
    }
    if (newSize == 0) {
        Deallocate(ptr);
        return nullptr;
    }
    void* newPtr = Allocate(newSize, alignment);
    const std::size_t oldSize = AllocationSize(ptr);
    std::memcpy(newPtr, ptr, std::min(oldSize, newSize));
    Deallocate(ptr);
    return newPtr;
}

void* TryReallocate(void* ptr, std::size_t newSize, std::size_t alignment = 16) {
    if (!ptr) {
        return TryAllocate(newSize, alignment);
    }
    if (newSize == 0) {
        Deallocate(ptr);
        return nullptr;
    }
    void* newPtr = TryAllocate(newSize, alignment);
    if (!newPtr) {
        return nullptr;
    }
    const std::size_t oldSize = AllocationSize(ptr);
    std::memcpy(newPtr, ptr, std::min(oldSize, newSize));
    Deallocate(ptr);
    return newPtr;
}

void FillRandomBytes(void* buffer, std::size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    std::random_device rd;
    auto* dst = static_cast<std::uint8_t*>(buffer);
    while (size >= sizeof(unsigned int)) {
        const unsigned int val = rd();
        std::memcpy(dst, &val, sizeof(val));
        dst += sizeof(val);
        size -= sizeof(val);
    }
    if (size > 0) {
        const unsigned int val = rd();
        std::memcpy(dst, &val, size);
    }
}

}  // namespace NKWebKit

extern "C" {
void* APS5_VABI _ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(std::size_t size, std::uint32_t) {
    return NKWebKit::Allocate(size);
}

void APS5_VABI _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(void* ptr, std::uint32_t) {
    NKWebKit::Deallocate(ptr);
}

void* APS5_VABI _ZN7bmalloc3api29tryLargeZeroedMemalignVirtualEmmNS_8HeapKindE(std::size_t alignment, std::size_t size, std::uint32_t) {
    void* ptr = NKWebKit::TryAllocate(size, alignment);
    if (ptr) {
        std::memset(ptr, 0, size);
    }
    return ptr;
}

void APS5_VABI _ZN7bmalloc3api8scavengeEv() {
}

bool APS5_VABI _ZN7bmalloc3api9isEnabledENS_8HeapKindE(std::uint32_t) {
    return true;
}

void APS5_VABI _ZN7bmalloc3api21commitAlignedPhysicalEPvmNS_8HeapKindE(void*, std::size_t, std::uint32_t) {
}

void APS5_VABI _ZN7bmalloc3api23decommitAlignedPhysicalEPvmNS_8HeapKindE(void*, std::size_t, std::uint32_t) {
}

void APS5_VABI _ZN7bmalloc3api14enableMiniModeEv() {
}

void APS5_VABI _ZN7bmalloc3api16disableScavengerEv() {
}

std::size_t APS5_VABI _ZN7bmalloc15availableMemoryEv() {
    return static_cast<std::size_t>(16ULL * 1024ULL * 1024ULL * 1024ULL);
}

void APS5_VABI _ZN7bmalloc12cryptoRandomEPvm(void* buffer, std::size_t size) {
    NKWebKit::FillRandomBytes(buffer, size);
}

bool APS5_VABI _ZN7bmalloc16isActiveHeapKindENS_8HeapKindE(std::uint32_t kind) {
    return kind == 0;
}

std::uint32_t APS5_VABI _ZN7bmalloc19mapToActiveHeapKindENS_8HeapKindE(std::uint32_t) {
    return 0;
}

}
