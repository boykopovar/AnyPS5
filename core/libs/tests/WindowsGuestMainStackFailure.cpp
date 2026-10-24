#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestStacks.hpp"
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <stdexcept>
#include <windows.h>

enum class Failure { Allocate, Commit, Pin };
static Failure failure;
static void* allocation;
static std::size_t allocatedBytes;
static unsigned pins;
static unsigned resets;
static unsigned releases;

static void Require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Main-stack failure check at line %u, mode %d\n", where.line(), static_cast<int>(failure));
        std::abort();
    }
}

namespace GuestArena {

void* GuestArenaAllocate_nid_postfix(std::size_t bytes, std::size_t) {
    if (failure == Failure::Allocate) throw std::runtime_error("allocation failure");
    Require(allocation == nullptr);
    allocation = VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
    Require(allocation != nullptr);
    allocatedBytes = bytes;
    return allocation;
}

void GuestArenaCommit_nid_postfix(void* pointer, std::size_t bytes, std::uint32_t protection, std::size_t) {
    const auto committed = failure == Failure::Commit ? 0x1000 : bytes;
    Require(VirtualAlloc(pointer, committed, MEM_COMMIT, protection) == pointer);
    if (failure == Failure::Commit) throw std::runtime_error("partial commit failure");
}

void GuestArenaPinWritable_nid_postfix(const void*, std::size_t) {
    ++pins;
    throw std::runtime_error("pin failure");
}

void GuestArenaUnpinWritable_nid_postfix(const void*, std::size_t) {
    if (pins != 0) --pins;
}

void GuestArenaReset_nid_postfix(void* pointer, std::size_t bytes) {
    Require(pointer == allocation && bytes == allocatedBytes);
    Require(VirtualFree(pointer, bytes, MEM_DECOMMIT) != 0);
    ++resets;
}

void GuestArenaRelease_nid_postfix(const void* pointer, std::size_t bytes) {
    Require(pointer == allocation && bytes == allocatedBytes && pins == 0);
    Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
    allocation = nullptr;
    ++releases;
}

}

static void UnexpectedEntry() { std::abort(); }

int main() {
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    const auto base = tib->StackBase;
    const auto allocationBase = *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<unsigned char*>(tib) + 0x1478);
    for (auto mode : {Failure::Allocate, Failure::Commit, Failure::Pin, Failure::Commit}) {
        failure = mode;
        const auto before = releases;
        try {
            GuestStacks::EnterGuestMainStack_nid_no_patch(UnexpectedEntry);
            std::abort();
        } catch (const std::runtime_error&) {
        }
        Require(allocation == nullptr && pins == 0 && releases == resets);
        Require(releases == before + (mode == Failure::Allocate ? 0 : 1));
        Require(tib->StackBase == base);
        Require(*reinterpret_cast<std::uintptr_t*>(reinterpret_cast<unsigned char*>(tib) + 0x1478) == allocationBase);
        Require(reinterpret_cast<std::uintptr_t>(tib->StackLimit) >= allocationBase);
        Require(reinterpret_cast<std::uintptr_t>(&mode) >= reinterpret_cast<std::uintptr_t>(tib->StackLimit));
    }
}
