#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr std::size_t Small = 0x1000;
constexpr std::uint32_t ReadOnly = 0x02;
constexpr std::uint32_t ReadWrite = 0x04;
constexpr std::uint32_t NoAccess = 0x01;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("guest arena Windows protection regression at " + std::to_string(location.line()));
}

DWORD HostProtection(const void* address) {
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory));
    return memory.Protect;
}

void ProtectsCommittedPrivateMemory() {
    constexpr std::size_t bytes = 4 * Page;
    auto* base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Page));
    Require(base != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(base, bytes, ReadWrite, Page);
    base[0] = 1;
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    GuestArena::GuestArenaSetProtection_nid_postfix(address + Small, Page, ReadOnly);
    Require(HostProtection(base) == PAGE_READWRITE);
    Require(HostProtection(base + Small) == PAGE_READONLY);
    Require(HostProtection(base + Page + Small - 1) == PAGE_READONLY);
    Require(HostProtection(base + Page + Small) == PAGE_READWRITE);
    Require(!GuestWritable_nid_no_patch(base + Small, 4));
    Require(GuestWritable_nid_no_patch(base, 4));
    GuestArena::GuestArenaSetProtection_nid_postfix(address + Small, Page, NoAccess);
    Require(HostProtection(base + Small) == PAGE_NOACCESS);
    GuestArena::GuestArenaSetProtection_nid_postfix(address + Small, Page, ReadWrite);
    Require(HostProtection(base + Small) == PAGE_READWRITE);
    Require(GuestWritable_nid_no_patch(base + Small, 4));
    base[Small] = 2;
    Require(base[0] == 1 && base[Small] == 2);
    GuestArena::GuestArenaReset_nid_postfix(base, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(base, bytes);
}

void LeavesReservedMemoryAlone() {
    constexpr std::size_t bytes = 2 * Page;
    auto* base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Page));
    Require(base != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(base, Page, ReadWrite, Page);
    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(base), bytes, ReadOnly);
    Require(HostProtection(base) == PAGE_READONLY);
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(base + Page, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_RESERVE);
    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(base), bytes, ReadWrite);
    GuestArena::GuestArenaReset_nid_postfix(base, Page);
    GuestArena::GuestArenaRelease_nid_postfix(base, bytes);
}

}

int main() {
    try {
        if (!GuestArena::GuestArenaAvailable_nid_postfix()) {
            std::puts("skipped, the guest arena is not available");
            return 77;
        }
        ProtectsCommittedPrivateMemory();
        LeavesReservedMemoryAlone();
        std::puts("guest arena Windows protection tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
