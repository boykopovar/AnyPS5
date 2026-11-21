#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>

extern "C" {

void* JemspaceMapPages_nid_no_patch(void* address, std::size_t bytes, std::size_t alignment, bool committed) {
    const int protection = committed ? 3 : 0;
    void* mapped;
    if (address || alignment <= 16384) {
        mapped = mmap_nid_postfix(address, bytes, protection, address ? 0x5012 : 0x1002, -1, 0);
    } else {
        void* reserved = nullptr;
        if (sceKernelReserveVirtualRange(&reserved, bytes, 0, alignment) != 0) {
            return nullptr;
        }
        mapped = mmap_nid_postfix(reserved, bytes, protection, 0x1012, -1, 0);
        if (mapped == reinterpret_cast<void*>(-1)) {
            sceKernelMunmap(reinterpret_cast<std::uintptr_t>(reserved), bytes);
        }
    }
    return mapped == reinterpret_cast<void*>(-1) ? nullptr : mapped;
}

void JemspaceUnmapPages_nid_no_patch(void* address, std::size_t bytes) {
    const int result = sceKernelMunmap(reinterpret_cast<std::uintptr_t>(address), bytes);
    if (result != 0) {
        std::fprintf(stderr, "Jemspace page unmap failed: 0x%x\n", static_cast<unsigned>(result));
    }
}

bool JemspaceSetPages_nid_no_patch(void* address, std::size_t bytes, bool committed) {
    void* mapped = mmap_nid_postfix(address, bytes, committed ? 3 : 0, 0x1012, -1, 0);
    if (mapped == reinterpret_cast<void*>(-1)) {
        return true;
    }
    if (mapped != address) {
        JemspaceUnmapPages_nid_no_patch(mapped, bytes);
        return true;
    }
    sceKernelSetVirtualRangeName(address, bytes, "jemalloc page");
    return false;
}

bool JemspacePurgePages_nid_no_patch(void* address, std::size_t bytes) {
    return madvise_nid_postfix(address, bytes, 5) != 0;
}
}
