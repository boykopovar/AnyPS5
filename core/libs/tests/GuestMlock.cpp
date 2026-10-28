#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(__linux__)
#include <fstream>
#include <string>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

static void RequireLockAllInvalid(int flags) {
    *__error_nid_postfix() = 0;
    Require(mlockall_nid_postfix(flags) == -1 && *__error_nid_postfix() == 22);
}

static void RequireLockAllUnsupported(int flags) {
    bool rejected = false;
    try {
        mlockall_nid_postfix(flags);

    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
}

#if defined(__linux__) && !defined(ANYPS5_ENABLE_ASAN) && !defined(ANYPS5_ENABLE_TSAN)
#define HAS_LOCKED_KILOBYTES
static std::size_t LockedKilobytes() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmLck:", 0) == 0) return std::strtoull(line.c_str() + 6, nullptr, 10);
    }
    return 0;
}
#endif

#ifdef __APPLE__
static unsigned WiredCount(const void* page) {
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(page);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    Require(mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) == KERN_SUCCESS);
    Require(address <= reinterpret_cast<mach_vm_address_t>(page));
    return info.user_wired_count;
}
#endif

int main() {
    constexpr int untouched = 12345;
    constexpr std::size_t page = 0x4000;
#ifdef _WIN32
    constexpr std::size_t length = 0x400000;
#else
    constexpr std::size_t length = 0x10000;
#endif
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);

    *__error_nid_postfix() = untouched;
    Require(mlock_nid_postfix(mapped, 0) == 0);
    Require(*__error_nid_postfix() == untouched);
#ifdef HAS_LOCKED_KILOBYTES
    const auto lockedBefore = LockedKilobytes();
#endif
    Require(mlock_nid_postfix(bytes + 1, length - page) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() - lockedBefore == length / 1024);
#endif
    Require(mlock_nid_postfix(mapped, length) == 0);
    Require(mlock_nid_postfix(mapped, length) == 0);
#ifdef __APPLE__
    Require(WiredCount(mapped) == 1);
    void* remapped = mapped;
    Require(sceKernelMapFlexibleMemory(&remapped, length, 3, 0x10) == 0 && remapped == mapped);
    Require(WiredCount(mapped) == 0);
    Require(mlock_nid_postfix(mapped, length) == 0);
    Require(WiredCount(mapped) == 1);
#endif
    bytes[length - 1] = 7;

    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(mapped, 0) == 0);
    Require(*__error_nid_postfix() == untouched);
    Require(munlock_nid_postfix(bytes + 1, length - page) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, length) && GetLastError() == ERROR_NOT_LOCKED);
#endif
    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(mapped, length) == 0);
    Require(*__error_nid_postfix() == untouched);
#ifdef __APPLE__
    Require(WiredCount(mapped) == 0);
#endif
    bytes[0] = 7;
    Require(mlock_nid_postfix(mapped, page) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() - lockedBefore == page / 1024);
#endif
    Require(munlock_nid_postfix(mapped, length) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, page) && GetLastError() == ERROR_NOT_LOCKED);
#endif
    Require(mlock_nid_postfix(mapped, page) == 0);
    Require(munlock_nid_postfix(mapped, page) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, page) && GetLastError() == ERROR_NOT_LOCKED);
#endif
    Require(mlock_nid_postfix(mapped, page) == 0);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() - lockedBefore == page / 1024);
#endif
    Require(mlock_nid_postfix(mapped, page) == 0);
    *__error_nid_postfix() = untouched;
    Require(munlockall_nid_postfix() == 0);
    Require(*__error_nid_postfix() == untouched);
#ifdef HAS_LOCKED_KILOBYTES
    Require(LockedKilobytes() == lockedBefore);
#elif defined(__APPLE__)
    Require(WiredCount(mapped) == 0);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, page) && GetLastError() == ERROR_NOT_LOCKED);
#endif

    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(munlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == -1);
    Require(*__error_nid_postfix() == 22);

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0);
#if !defined(ANYPS5_ENABLE_ASAN) && !defined(ANYPS5_ENABLE_TSAN)
    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(reserved, page) == -1 && *__error_nid_postfix() == 12);
#endif
    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(reserved, page) == 0);
    Require(*__error_nid_postfix() == untouched);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), page) == 0);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), length) == 0);
#if !defined(ANYPS5_ENABLE_ASAN) && !defined(ANYPS5_ENABLE_TSAN)
    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(mapped, page) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(munlock_nid_postfix(mapped, page) == -1 && *__error_nid_postfix() == 12);
#endif

    RequireLockAllInvalid(0);
    RequireLockAllInvalid(4);
    RequireLockAllInvalid(1 | 4);
    RequireLockAllInvalid(-1);
    RequireLockAllUnsupported(1);
    RequireLockAllUnsupported(2);
    RequireLockAllUnsupported(1 | 2);

    *__error_nid_postfix() = untouched;
    Require(munlockall_nid_postfix() == 0);
    Require(*__error_nid_postfix() == untouched);
    return 0;
}
