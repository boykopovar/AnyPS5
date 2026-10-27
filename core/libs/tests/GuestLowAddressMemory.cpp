#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Low guest memory check failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bufferBytes = 64u << 20;
    constexpr std::size_t stackBytes = 128u << 20;
    constexpr std::uintptr_t lowLimit = 0x80000000;
    const auto failed = reinterpret_cast<void*>(~std::uintptr_t{0});
    auto* buffer = static_cast<unsigned char*>(mmap_nid_postfix(reinterpret_cast<void*>(0x20000000), bufferBytes, 3, 0x1002, -1, 0));
    auto* stack = static_cast<unsigned char*>(mmap_nid_postfix(reinterpret_cast<void*>(0x40000000), stackBytes, 3, 0x1002, -1, 0));
    std::fprintf(stderr, "Low guest memory: buffer=%p stack=%p\n", static_cast<void*>(buffer), static_cast<void*>(stack));
    Require(buffer != failed && stack != failed);
    const auto bufferAddress = reinterpret_cast<std::uintptr_t>(buffer);
    const auto stackAddress = reinterpret_cast<std::uintptr_t>(stack);
    Require(bufferAddress >= 0x20000000 && bufferAddress <= lowLimit - bufferBytes);
    Require(stackAddress >= 0x40000000 && stackAddress <= lowLimit - stackBytes);
    Require(GuestArena::GuestArenaContains_nid_postfix(buffer, bufferBytes));
    Require(GuestArena::GuestArenaContains_nid_postfix(stack, stackBytes));
    buffer[0] = 0x31;
    buffer[bufferBytes - 1] = 0x32;
    stack[0] = 0x41;
    stack[stackBytes - 1] = 0x42;

    void* collision = mmap_nid_postfix(buffer, page, 3, 0x1002, -1, 0);
    Require(collision != failed && collision != buffer);
    Require(buffer[0] == 0x31 && buffer[bufferBytes - 1] == 0x32);
    Require(munmap_nid_postfix(collision, page) == 0);
    Require(mmap_nid_postfix(buffer, page, 3, 0x5012, -1, 0) == buffer);
    Require(buffer[0] == 0 && buffer[bufferBytes - 1] == 0x32);
    buffer[0] = 0x31;
    Require(mmap_nid_postfix(buffer, page, 3, 0x1012, -1, 0) == buffer);
    Require(buffer[0] == 0 && buffer[bufferBytes - 1] == 0x32);

    Require(mprotect_nid_postfix(buffer, page, 1) == 0);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(buffer, 0, &info, sizeof(info)) == 0 && info.protection == 1);
    Require(mprotect_nid_postfix(buffer, page, 3) == 0);
    buffer[0] = 0x51;

    void* ordinary = mmap_nid_postfix(nullptr, page, 3, 0x1002, -1, 0);
    Require(ordinary != failed && reinterpret_cast<std::uintptr_t>(ordinary) >= 0x200000000ull);
    Require(munmap_nid_postfix(ordinary, page) == 0);
    Require(munmap_nid_postfix(buffer, bufferBytes) == 0);
    Require(munmap_nid_postfix(stack, stackBytes) == 0);
    Require(mmap_nid_postfix(buffer, page, 3, 0x1002, -1, 0) == buffer);
    Require(buffer[0] == 0);
    Require(munmap_nid_postfix(buffer, page) == 0);
}
