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

    phys = -1;
    Require(sceKernelAllocateDirectMemoryForApp(0, size, page * 4, 0, 0, &phys) == 0);
    Require(phys >= 0);
    Require((phys % static_cast<int64_t>(page)) == 0);
    Require(phys + static_cast<int64_t>(page * 4) <= size);
    std::printf("allocated for-app phys=%lld len=%zu\n", static_cast<long long>(phys), page * 4);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);

    return 0;
}
