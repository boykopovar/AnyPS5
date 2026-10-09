#include "DirectMemory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <limits>
#include <new>
#include <stdexcept>

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI sceKernelMlock_nid_postfix(void* address, std::uint64_t length);
extern "C" int APS5_VABI sceKernelMunlock_nid_postfix(void* address, std::uint64_t length);

namespace {
// FreeBSD/PS5 ABI values, independent of the host's errno and mmap constants.
constexpr int GuestInvalid = 22;
constexpr int GuestNoMemory = 12;
constexpr int GuestNotSupported = 45;
constexpr int GuestPrivate = 0x2;
constexpr int GuestAnonymous = 0x1000;

bool RoundLength(std::size_t length, std::size_t& rounded) {
    constexpr auto mask = PS5_PAGE_SIZE - 1;
    if (length == 0 || length > std::numeric_limits<std::size_t>::max() - mask)
        return false;
    rounded = (length + mask) & ~mask;
    return true;
}

void SetError(int error) {
    *__error_nid_postfix() = error;
}
}

extern "C" {

void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection,
                                int flags, int descriptor, std::int64_t offset) noexcept {
    const auto failed = [](int error) -> void* {
        SetError(error);
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    };
    std::size_t rounded;
    if (!RoundLength(length, rounded) || (protection & ~7) != 0)
        return failed(GuestInvalid);
    if (flags != (GuestPrivate | GuestAnonymous))
        return failed(GuestNotSupported);
    if (descriptor != -1 || offset != 0)
        return failed(GuestInvalid);
    // A non-fixed address is a hint; the kernel may choose another address.
    (void)address;
    void* mapped = nullptr;
    try {
        const auto result = DoMapAnon(&mapped, rounded, protection, 0);
        if (result != 0) return failed(GuestNoMemory);
        return mapped;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestNoMemory);
    }
}

int APS5_VABI munmap_nid_postfix(void* address, std::size_t length) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    std::size_t rounded;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (!RoundLength(length, rounded) || start == 0 ||
        (start & (PS5_PAGE_SIZE - 1)) != 0 ||
        rounded > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMunmap(address, rounded) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        // The allocation tracker rejects foreign, pinned, and noncontiguous ranges.
        return failed(GuestInvalid);
    }
}

int APS5_VABI mprotect_nid_postfix(void* address, std::size_t length, int protection) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (length == 0) return 0;
    if (start == 0 || length > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMprotect(address, length, protection) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestInvalid);
    }
}

int APS5_VABI mlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI munlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMunlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

}
