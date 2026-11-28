#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::size_t GuestPageSize = 0x4000;
constexpr int ReadWrite = 3;
constexpr int MapFixed = 0x10;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest kernel map guest pointers check failed at %s:%u\n",
            location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

std::size_t HostPageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

void** UnmappedSlot() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, HostPageSize(), MEM_RESERVE, PAGE_NOACCESS);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, HostPageSize(), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<void**>(page);
}

void** ReadOnlySlot(void* value) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, HostPageSize(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
    *static_cast<void**>(page) = value;
    DWORD old = 0;
    Require(VirtualProtect(page, HostPageSize(), PAGE_READONLY, &old) != 0);
#else
    void* page = mmap(nullptr, HostPageSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    *static_cast<void**>(page) = value;
    Require(mprotect(page, HostPageSize(), PROT_READ) == 0);
#endif
    return static_cast<void**>(page);
}

std::size_t AvailableFlexible() {
    std::size_t size = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&size) == 0);
    return size;
}

void Unmap(void* address) {
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(address), GuestPageSize) == 0);
}

void CheckFlexible() {
    void** unmapped = UnmappedSlot();
    void** readOnly = ReadOnlySlot(nullptr);
    const std::size_t before = AvailableFlexible();
    Require(sceKernelMapFlexibleMemory(unmapped, GuestPageSize, ReadWrite, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapFlexibleMemory(readOnly, GuestPageSize, ReadWrite, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapNamedFlexibleMemory(unmapped, GuestPageSize, ReadWrite, 0, "slot") == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapNamedFlexibleMemory(readOnly, GuestPageSize, ReadWrite, 0, "slot") == SCE_KERNEL_ERROR_EFAULT);
    Require(*readOnly == nullptr && AvailableFlexible() == before);

    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, GuestPageSize, ReadWrite, 0) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[GuestPageSize - 1] = 1;
    Unmap(mapped);
    void* fixed = mapped;
    Require(sceKernelMapNamedFlexibleMemory(&fixed, GuestPageSize, ReadWrite, MapFixed, "slot") == 0 && fixed == mapped);
    Unmap(fixed);
}

void CheckDirect() {
    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), GuestPageSize, GuestPageSize, 0, &physical) == 0);
    void** unmapped = UnmappedSlot();
    void** readOnly = ReadOnlySlot(nullptr);
    Require(sceKernelMapDirectMemory(unmapped, GuestPageSize, ReadWrite, 0, physical, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapDirectMemory(readOnly, GuestPageSize, ReadWrite, 0, physical, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapDirectMemory2(unmapped, GuestPageSize, 0, ReadWrite, 0, physical, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMapDirectMemory2(readOnly, GuestPageSize, 0, ReadWrite, 0, physical, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(*readOnly == nullptr);

    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, GuestPageSize, ReadWrite, 0, physical, 0) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[0] = 2;
    Unmap(mapped);
    void* fixed = mapped;
    Require(sceKernelMapDirectMemory2(&fixed, GuestPageSize, 0, ReadWrite, MapFixed, physical, 0) == 0 && fixed == mapped);
    Require(static_cast<volatile unsigned char*>(fixed)[0] == 2);
    Unmap(fixed);
    Require(sceKernelReleaseDirectMemory(physical, GuestPageSize) == 0);
}

void CheckReserve() {
    void** unmapped = UnmappedSlot();
    void** readOnly = ReadOnlySlot(nullptr);
    Require(sceKernelReserveVirtualRange(unmapped, GuestPageSize, 0, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelReserveVirtualRange(readOnly, GuestPageSize, 0, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(*readOnly == nullptr);

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, GuestPageSize, 0, 0) == 0 && reserved != nullptr);
    void* fixed = reserved;
    Require(sceKernelMapFlexibleMemory(&fixed, GuestPageSize, ReadWrite, MapFixed) == 0 && fixed == reserved);
    Unmap(fixed);
}

} // namespace

int main() {
    CheckFlexible();
    CheckDirect();
    CheckReserve();
    return 0;
}
