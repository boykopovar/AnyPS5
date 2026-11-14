#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GuestKernelMprotectNone check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = 0x4000;
    constexpr int protReadWrite = 3;
    constexpr int protNone = 0;

    void* memory = nullptr;
    Require(sceKernelReserveVirtualRange(&memory, 2 * page, 0, page) == 0);
    Require(memory != nullptr);

    void* mapped = memory;
    Require(sceKernelMapFlexibleMemory(&mapped, page, protReadWrite, 0x10) == 0);
    Require(mapped == memory);

    volatile std::uint64_t* ptr = reinterpret_cast<volatile std::uint64_t*>(memory);
    *ptr = 0x5a5a5a5a5a5a5a5aULL;
    Require(*ptr == 0x5a5a5a5a5a5a5a5aULL);

    Require(sceKernelMprotect(memory, page, protNone) == 0);
    Require(mprotect_nid_postfix(memory, page, protReadWrite) == 0);
    Require(*ptr == 0x5a5a5a5a5a5a5a5aULL);

    Require(sceKernelMprotect(memory, page, protNone) == 0);
    Require(sceKernelMprotect(memory, page, protReadWrite) == 0);
    Require(*ptr == 0x5a5a5a5a5a5a5a5aULL);

    Require(sceKernelMprotect(static_cast<char*>(memory) + 1, page, protNone) != 0);
    Require(sceKernelMprotect(memory, 0, protNone) == 0);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory), 2 * page) == 0);
    return 0;
}
