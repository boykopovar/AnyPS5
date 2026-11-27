#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const std::source_location& location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "unwind module info pointer test failed at line %u\n", location.line());
        std::abort();
    }
}

void Anchor() {}

class Pages {
    unsigned char* allocation = nullptr;
    std::size_t size = 0;

public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(allocation + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + size, size, PROT_NONE) == 0);
#endif
        char value = 0;
        Require(!GuestCopyIn_nid_no_patch(&value, allocation + size, 1));
        Require(!GuestCopyOut_nid_no_patch(allocation + size, &value, 1));
    }

    ~Pages() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(allocation, size * 2) == 0);
#endif
    }

    unsigned char* Begin() { return allocation; }
    unsigned char* End() { return allocation + size; }
    std::size_t PageSize() const { return size; }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(allocation, size, PROT_READ) == 0);
#endif
    }

    void ProtectReadWrite() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(allocation, size, PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(allocation, size, PROT_READ | PROT_WRITE) == 0);
#endif
    }
};

std::uint64_t MissingAddress(std::size_t pageSize) {
#ifdef _WIN32
    (void)pageSize;
    return 0xffff800000000000ull;
#else
    void* unmapped = mmap(nullptr, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(unmapped != MAP_FAILED);
    const auto address = reinterpret_cast<std::uint64_t>(unmapped);
    Require(munmap(unmapped, pageSize) == 0);
    return address;
#endif
}

void CheckPreservesErrno(int expected, int result) {
    Require(result == expected);
    Require(errno == 73);
    Require(*__error_nid_postfix() == 73);
}

void SetTestErrno() {
    errno = 73;
    *__error_nid_postfix() = 73;
}

}

int main() {
    Pages pages;
    Require(pages.PageSize() > sizeof(ModuleInfoForUnwind));
    const auto validAddr = reinterpret_cast<std::uint64_t>(&Anchor);
    const auto missingAddr = MissingAddress(pages.PageSize());

    ModuleInfoForUnwind baseline{};
    baseline.st_size = sizeof(ModuleInfoForUnwind);
    SetTestErrno();
    CheckPreservesErrno(0, sceKernelGetModuleInfoForUnwind(validAddr, 1, &baseline));
    Require(baseline.st_size == sizeof(ModuleInfoForUnwind));
    Require(baseline.seg0_size != 0);

    ModuleInfoForUnwind absent{};
    std::memset(&absent, 0xab, sizeof(absent));
    absent.st_size = sizeof(ModuleInfoForUnwind);
    SetTestErrno();
    CheckPreservesErrno(SCE_KERNEL_ERROR_ESRCH, sceKernelGetModuleInfoForUnwind(missingAddr, 1, &absent));
    const auto* absentBytes = reinterpret_cast<const unsigned char*>(&absent);
    Require(std::all_of(absentBytes, absentBytes + sizeof(absent), [](unsigned char b) { return b == 0; }));

    auto* guardInfo = reinterpret_cast<ModuleInfoForUnwind*>(pages.End());
    for (const auto& [addr, flags] : {
             std::pair{validAddr, 0},
             std::pair{validAddr, 1},
             std::pair{missingAddr, 1},
             std::pair{validAddr, 2},
         }) {
        SetTestErrno();
        CheckPreservesErrno(SCE_KERNEL_ERROR_EFAULT, sceKernelGetModuleInfoForUnwind(addr, flags, guardInfo));
    }

    auto* roInfo = reinterpret_cast<ModuleInfoForUnwind*>(pages.Begin());
    roInfo->st_size = sizeof(ModuleInfoForUnwind) - 1;
    pages.ProtectReadOnly();
    SetTestErrno();
    CheckPreservesErrno(SCE_KERNEL_ERROR_EINVAL, sceKernelGetModuleInfoForUnwind(validAddr, 1, roInfo));

    pages.ProtectReadWrite();
    roInfo->st_size = sizeof(ModuleInfoForUnwind);
    pages.ProtectReadOnly();
    for (const auto& [addr, flags] : {
             std::pair{validAddr, 0},
             std::pair{validAddr, 1},
             std::pair{missingAddr, 1},
         }) {
        SetTestErrno();
        CheckPreservesErrno(SCE_KERNEL_ERROR_EFAULT, sceKernelGetModuleInfoForUnwind(addr, flags, roInfo));
    }

    pages.ProtectReadWrite();
    auto* partial = reinterpret_cast<ModuleInfoForUnwind*>(pages.End() - sizeof(ModuleInfoForUnwind) + 16);
    std::memset(partial, 0x5a, sizeof(ModuleInfoForUnwind) - 16);
    partial->st_size = sizeof(ModuleInfoForUnwind);
    for (const auto& [addr, flags] : {
             std::pair{validAddr, 1},
             std::pair{missingAddr, 1},
             std::pair{validAddr, 0},
         }) {
        SetTestErrno();
        CheckPreservesErrno(SCE_KERNEL_ERROR_EFAULT, sceKernelGetModuleInfoForUnwind(addr, flags, partial));
    }

    ModuleInfoForUnwind wide{};
    std::memset(&wide, 0xab, sizeof(wide));
    wide.st_size = 0x200;
    SetTestErrno();
    CheckPreservesErrno(0, sceKernelGetModuleInfoForUnwind(validAddr, 1, &wide));
    Require(wide.st_size == 0x200);
    wide.st_size = baseline.st_size;
    Require(std::memcmp(&wide, &baseline, sizeof(wide)) == 0);
}
