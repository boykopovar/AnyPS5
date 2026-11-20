#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Direct memory initialization check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    const auto size = static_cast<int64_t>(DirectMemorySize());
    int64_t start = -1;
    std::size_t available = 123;

    Require(sceKernelAvailableDirectMemorySize(0, size, 3, &start, &available) == SCE_KERNEL_ERROR_EINVAL);
    Require(start == -1 && available == 123);
    Require(sceKernelAvailableDirectMemorySize(0, size, PS5_PAGE_SIZE, &start, &available) == SCE_KERNEL_ERROR_EBADF);
    Require(start == -1 && available == 123);

    const auto reportedSize = sceKernelGetDirectMemorySize();
    Require(reportedSize == static_cast<std::size_t>(size));

    start = -1;
    available = 0;
    Require(sceKernelAvailableDirectMemorySize(0, static_cast<int64_t>(reportedSize), PS5_PAGE_SIZE, &start, &available) == 0);
    Require(start == 0 && available == reportedSize);

    return 0;
}