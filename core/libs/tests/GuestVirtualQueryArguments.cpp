#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr unsigned char Fill = 0xA5;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sceKernelVirtualQuery check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

std::size_t HighestWritten(const unsigned char* buffer, std::size_t bytes) {
    std::size_t highest = 0;
    for (std::size_t index = 0; index < bytes; ++index) {
        if (buffer[index] != Fill) highest = index + 1;
    }
    return highest;
}

void CheckFlags(void* mapped) {
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(mapped, 0, &info, sizeof(info)) == 0);
    Require(sceKernelVirtualQuery(mapped, 1, &info, sizeof(info)) == 0);
    for (const int flags : {2, 3, 4, 8, 0x10, 0x100, -1, static_cast<int>(0x80000000u)}) {
        Require(sceKernelVirtualQuery(mapped, flags, &info, sizeof(info)) == SCE_KERNEL_ERROR_EINVAL);
        Require(sceKernelVirtualQuery(mapped, flags, nullptr, sizeof(info)) == SCE_KERNEL_ERROR_EINVAL);
        Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), flags, &info, sizeof(info)) == SCE_KERNEL_ERROR_EINVAL);
        Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(~std::uintptr_t{0}), flags, &info, sizeof(info)) == SCE_KERNEL_ERROR_EINVAL);
    }
}

void CheckAddresses() {
    VirtualQueryInfo info{};
    for (const std::uintptr_t address : {std::uintptr_t{0x800000000000ull}, std::uintptr_t{0x800000000001ull}, std::uintptr_t{0x800000003fffull}}) {
        const auto* pointer = reinterpret_cast<const void*>(address);
        for (const int flags : {0, 1}) {
            Require(sceKernelVirtualQuery(pointer, flags, &info, sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
            Require(sceKernelVirtualQuery(pointer, flags, nullptr, 0) == SCE_KERNEL_ERROR_EACCES);
            Require(sceKernelVirtualQuery(pointer, flags, reinterpret_cast<VirtualQueryInfo*>(1), sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
        }
        Require(sceKernelVirtualQuery(pointer, 2, nullptr, 0) == SCE_KERNEL_ERROR_EINVAL);
    }
    Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), 0, &info, sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
    for (const std::uintptr_t address : {std::uintptr_t{0x800000004000ull}, std::uintptr_t{0x1000000000000ull}, std::uintptr_t{0x7fffffffffffffffull}, std::uintptr_t{0xffff800000000000ull}, std::uintptr_t{0xffffffff80000000ull}, ~std::uintptr_t{0}}) {
        for (const int flags : {0, 1}) {
            Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(address), flags, &info, sizeof(info)) == SCE_KERNEL_ERROR_EINVAL);
            Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(address), flags, nullptr, 0) == SCE_KERNEL_ERROR_EINVAL);
        }
    }
}

void CheckOutput(void* mapped) {
    VirtualQueryInfo full{};
    Require(sceKernelVirtualQuery(mapped, 0, &full, sizeof(full)) == 0);
    Require(full.start == reinterpret_cast<std::uintptr_t>(mapped) && full.end == full.start + 4 * Page);

    for (const std::size_t size : {std::size_t{1}, std::size_t{8}, std::size_t{24}, std::size_t{48}, std::size_t{71}, std::size_t{72}, std::size_t{73}, std::size_t{128}, std::size_t{4096}}) {
        unsigned char buffer[4096];
        std::memset(buffer, Fill, sizeof(buffer));
        Require(sceKernelVirtualQuery(mapped, 0, reinterpret_cast<VirtualQueryInfo*>(buffer), size) == 0);
        const std::size_t copied = size < sizeof(full) ? size : sizeof(full);
        Require(std::memcmp(buffer, &full, copied) == 0);
        Require(HighestWritten(buffer, sizeof(buffer)) <= copied);
        for (std::size_t index = copied; index < sizeof(buffer); ++index) Require(buffer[index] == Fill);
    }

    unsigned char untouched[sizeof(full)];
    std::memset(untouched, Fill, sizeof(untouched));
    Require(sceKernelVirtualQuery(mapped, 0, reinterpret_cast<VirtualQueryInfo*>(untouched), 0) == 0);
    Require(HighestWritten(untouched, sizeof(untouched)) == 0);
    Require(sceKernelVirtualQuery(mapped, 0, nullptr, 0) == 0);
    Require(sceKernelVirtualQuery(mapped, 1, nullptr, 0) == 0);

    Require(sceKernelVirtualQuery(mapped, 0, nullptr, sizeof(full)) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelVirtualQuery(mapped, 0, nullptr, 1) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelVirtualQuery(mapped, 0, reinterpret_cast<VirtualQueryInfo*>(1), sizeof(full)) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelVirtualQuery(mapped, 0, reinterpret_cast<VirtualQueryInfo*>(0xffffffff80000000ull), sizeof(full)) == SCE_KERNEL_ERROR_EFAULT);

    Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), 0, nullptr, sizeof(full)) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), 0, nullptr, 0) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), 0, reinterpret_cast<VirtualQueryInfo*>(untouched), 1) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelVirtualQuery(reinterpret_cast<const void*>(0x1000), 1, nullptr, sizeof(full)) == SCE_KERNEL_ERROR_EFAULT);
}

}

int main() {
    void* mapped = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&mapped, 4 * Page, 3, 0, "vq-args") == 0);
    CheckFlags(mapped);
    CheckAddresses();
    CheckOutput(mapped);
    return 0;
}
