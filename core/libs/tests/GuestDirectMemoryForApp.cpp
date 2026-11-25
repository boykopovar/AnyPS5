#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Direct memory for-app check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = PS5_PAGE_SIZE;
    const auto size = static_cast<int64_t>(sceKernelGetDirectMemorySize());
    int64_t phys = 0;

    Require(sceKernelAllocateDirectMemoryForApp(0, size, 0, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemoryForApp(0, size, page + 1, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemoryForApp(0, size, page, 0, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAllocateDirectMemoryForApp(size, 0, page, 0, 0, &phys) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelAllocateDirectMemoryForApp(0, size, page, 0, 0, &phys) == 0);
    const auto freePage = phys;
    const auto pageEnd = freePage + static_cast<int64_t>(page);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
    auto* const badOutput = reinterpret_cast<int64_t*>(1);
    Require(sceKernelAllocateDirectMemoryForApp(freePage, pageEnd, page, 0, 0, badOutput) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAllocateDirectMemoryForApp(freePage, pageEnd, page, 0, 0, &phys) == 0 && phys == freePage);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);

    constexpr std::size_t block = 2 * 1024 * 1024;
    Require(sceKernelAllocateDirectMemoryForApp(0, size, block, block, 0, &phys) == 0);
    const auto freeBlock = phys;
    const auto blockEnd = freeBlock + static_cast<int64_t>(block);
    Require(sceKernelReleaseDirectMemory(phys, block) == 0);
    Require(sceKernelMemoryPoolExpand(freeBlock, blockEnd, block, block, badOutput) == SCE_KERNEL_ERROR_EFAULT);
#ifdef _WIN32
    void* const readOnlyPage = VirtualAlloc(nullptr, page, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
    Require(readOnlyPage != nullptr);
#else
    void* const readOnlyPage = mmap(nullptr, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(readOnlyPage != MAP_FAILED);
#endif
    Require(sceKernelMemoryPoolExpand(freeBlock, blockEnd, block, block, static_cast<int64_t*>(readOnlyPage)) == SCE_KERNEL_ERROR_EFAULT);
#ifdef _WIN32
    Require(VirtualFree(readOnlyPage, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(readOnlyPage, page) == 0);
#endif
    KernelMemoryPoolBlockStats stats{};
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.available_flushed_blocks == 0 && stats.allocated_flushed_blocks == 0);
    Require(sceKernelMemoryPoolExpand(freeBlock, blockEnd, block, block, &phys) == 0 && phys == freeBlock);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.available_flushed_blocks == 1 && stats.allocated_flushed_blocks == 0);
    Require(sceKernelReleaseDirectMemory(phys, block) == 0);

    phys = -1;
    Require(sceKernelAllocateDirectMemoryForApp(0, size, page * 4, 0, 0, &phys) == 0);
    Require(phys >= 0);
    Require((phys % static_cast<int64_t>(page)) == 0);
    Require(phys + static_cast<int64_t>(page * 4) <= size);
    std::printf("allocated for-app phys=%lld len=%zu\n", static_cast<long long>(phys), page * 4);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);

    constexpr int64_t mib = 1024 * 1024;
    phys = -1;
    Require(sceKernelAllocateDirectMemory(mib, 0, mib, 0, 12, &phys) == 0);
    Require(phys >= mib);
    Require((phys % static_cast<int64_t>(page)) == 0);
    Require(phys + mib <= size);
    Require(sceKernelReleaseDirectMemory(phys, mib) == 0);

    return 0;
}
