#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr int ProtectNone = 0;
constexpr int ProtectRead = 1;
constexpr int ProtectWrite = 2;
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

unsigned char* Map(std::size_t pages) {
    void* raw = mmap_nid_postfix(nullptr, Page * pages, ProtectReadWrite, 0x1002, -1, 0);
    Require(raw != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)), "mmap failed");
    return static_cast<unsigned char*>(raw);
}

} // namespace

int main() {
    unsigned char* memory = Map(2);

    Require(Protect(memory, Page, ProtectNone) == 0, "mprotect PROT_NONE failed");
    Require(Protect(memory, Page, ProtectRead) == 0, "mprotect restore from PROT_NONE failed");

    Require(Protect(memory, std::numeric_limits<std::size_t>::max(), ProtectRead) < 0, "overflow size must fail");

    Require(munmap_nid_postfix(memory, Page * 2) == 0, "munmap failed");
    return 0;
}
