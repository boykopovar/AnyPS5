#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr int ErrorInvalid = 22;
constexpr int ProtectRead = 1;
constexpr int ProtectReadWrite = 3;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

int Protect(const void* address, std::size_t bytes, int protection) {
    try {
        return sceKernelMprotect(address, bytes, protection);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "sceKernelMprotect threw: %s\n", error.what());
        std::abort();
    }
}

int ProtectionAt(const unsigned char* address) {
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(address, 0, &info, sizeof(info)) == 0, "virtual query of a mapped page failed");
    return info.protection;
}

unsigned char* Map(std::size_t pages) {
    void* raw = mmap_nid_postfix(nullptr, Page * pages, ProtectReadWrite, 0x1002, -1, 0);
    Require(raw != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)), "mmap failed");
    return static_cast<unsigned char*>(raw);
}

}

int main() {
    unsigned char* memory = Map(3);
    Require(munmap_nid_postfix(memory + Page, Page) == 0, "munmap of the middle page failed");

    Require(Protect(memory, 0, ProtectRead) == 0, "a zero-length protection of a mapped page must succeed");
    Require(ProtectionAt(memory) == ProtectReadWrite, "a zero-length protection must not change the page");
    Require(Protect(memory + Page, 0, ProtectRead) == 0 && Protect(nullptr, 0, ProtectRead) == 0, "a zero-length protection of unmapped memory must succeed");
    Require(sceKernelMtypeprotect(memory, 0, 0, ProtectRead) == 0 && ProtectionAt(memory) == ProtectReadWrite, "a zero-length sceKernelMtypeprotect must succeed without changing the page");
    Require(mprotect_nid_postfix(memory + Page, 0, ProtectRead) == 0, "a zero-length mprotect of unmapped memory must succeed");

    Require(Protect(memory, Page * 3, ProtectRead) == 0, "a range with a gap in the middle must succeed for its mapped parts");
    Require(ProtectionAt(memory) == ProtectRead && ProtectionAt(memory + Page * 2) == ProtectRead, "the mapped parts on both sides of the gap must be protected");
    Require(mprotect_nid_postfix(memory, Page * 3, ProtectReadWrite) == 0, "mprotect over a range with a gap must succeed for its mapped parts");
    Require(ProtectionAt(memory) == ProtectReadWrite && ProtectionAt(memory + Page * 2) == ProtectReadWrite, "mprotect must change the mapped parts on both sides of the gap");
    Require(sceKernelMtypeprotect(memory, Page * 3, 0, ProtectRead) == 0, "sceKernelMtypeprotect over a range with a gap must succeed for its mapped parts");

    Require(Protect(memory + Page - 8, 16, ProtectReadWrite) == 0, "an unaligned range that reaches into the unmapped page protects the mapped page");
    Require(Protect(memory + 100, Page - 200, ProtectReadWrite) == 0, "an unaligned range inside the first page must work");

    Require(Protect(memory + Page, Page, ProtectRead) == SCE_KERNEL_ERROR_EINVAL, "a range that is completely unmapped must return EINVAL");
    Require(Protect(memory + Page * 2, Page, ProtectReadWrite) == 0, "protecting the last page alone must work");
    Require(Protect(memory + Page, Page * 2, ProtectRead) == SCE_KERNEL_ERROR_EINVAL, "a range whose start is unmapped must return EINVAL even when later pages are mapped");
    Require(ProtectionAt(memory + Page * 2) == ProtectReadWrite, "a refused range must not change the mapped pages behind its start");
    Require(mprotect_nid_postfix(memory + Page, Page, ProtectRead) == -1 && *__error_nid_postfix() == ErrorInvalid, "mprotect over a range that is completely unmapped must fail with EINVAL (22)");

    Require(munmap_nid_postfix(memory, Page) == 0 && munmap_nid_postfix(memory + Page * 2, Page) == 0, "munmap of the remaining pages failed");

    unsigned char* tail = Map(2);
    Require(munmap_nid_postfix(tail + Page, Page) == 0, "munmap of the last page failed");
    Require(Protect(tail, Page * 2, ProtectRead) == 0, "a range that ends in an unmapped page must succeed for its mapped part");
    Require(ProtectionAt(tail) == ProtectRead, "the mapped part before a trailing gap must be protected");
    Require(munmap_nid_postfix(tail, Page) == 0, "munmap of the remaining page failed");
    return 0;
}
