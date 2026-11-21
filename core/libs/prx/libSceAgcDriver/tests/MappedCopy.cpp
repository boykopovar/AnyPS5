#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

using namespace AgcDriver::GuestMemory;
constexpr std::size_t Unit = 65536;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Range {
public:
    explicit Range(std::size_t units = 2) : bytes(units * Unit) {
        data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Unit));
        Require(data != nullptr, "cannot reserve mapped-copy fixture");
        GuestArena::GuestArenaCommit_nid_postfix(data, bytes, 0x04, Unit);
        std::memset(data, 0x5b, bytes);
    }

    ~Range() {
        GuestArena::GuestArenaReset_nid_postfix(data, bytes);
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address(), bytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
    }

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::byte* Data() const { return data; }

private:
    const std::size_t bytes;
    std::byte* data = nullptr;
};

unsigned readGuards = 0;
unsigned flushes = 0;

void NoteRead(std::uint64_t, std::size_t) { ++readGuards; }
void NoteFlush(std::uint64_t, std::size_t) { ++flushes; }

void CheckCommitted() {
    Range range(3);
    std::array<std::byte, 3 * Unit - 48> out;
    struct ResetHooks {
        ~ResetHooks() {
            SetReadGuard(nullptr);
            SetFlushHook(nullptr);
        }
    } reset;
    readGuards = flushes = 0;
    SetReadGuard(&NoteRead);
    SetFlushHook(&NoteFlush);
    ReadCommittedCheckedUnsynced(0, {});
    ReadCommittedCheckedUnsynced(std::numeric_limits<std::uintptr_t>::max(), {});
    Require(readGuards == 0 && flushes == 0, "empty committed reads ran preparation hooks");
    ReadCommittedCheckedUnsynced(range.Address() + 17u, out);
    Require(std::all_of(out.begin(), out.end(), [](std::byte byte) { return byte == std::byte{0x5b}; }), "a checked committed read lost mapped bytes");
    Require(readGuards == 1 && flushes == 0, "a checked unsynced read lost its guard or flushed GPU writes");
    GuestArena::GuestArenaReset_nid_postfix(range.Data() + Unit, Unit);
    out.fill(std::byte{0xa5});
    errno = E2BIG;
    ReadCommittedCheckedUnsynced(range.Address() + 17u, out);
    Require(errno == E2BIG, "a checked committed read changed errno");
    for (std::size_t index = 0; index < out.size(); ++index) {
        const auto offset = index + 17u;
        const auto expected = offset >= Unit && offset < 2 * Unit ? std::byte{} : std::byte{0x5b};
        Require(out[index] == expected, "a partial checked copy lost a readable prefix or suffix, or retained bytes in a hole");
    }
    Require(readGuards == 2 && flushes == 0, "page fallback repeated preparation hooks");
    std::array<std::byte, 257> hole;
    hole.fill(std::byte{0xa5});
    ReadCommittedCheckedUnsynced(range.Address() + Unit + 13u, hole);
    Require(std::all_of(hole.begin(), hole.end(), [](std::byte byte) { return byte == std::byte{}; }), "an unreadable checked range was not zeroed");
    hole.fill(std::byte{0xa5});
    ReadCommittedCheckedUnsynced(0, hole);
    Require(std::all_of(hole.begin(), hole.end(), [](std::byte byte) { return byte == std::byte{}; }), "a null checked range was not zeroed");
#ifdef _WIN32
    constexpr std::size_t Page = 4096;
    DWORD previous = 0;
    Require(VirtualProtect(range.Data() + Page, Page, PAGE_READWRITE | PAGE_GUARD, &previous) != 0, "cannot protect a checked-read guard page");
    hole.fill(std::byte{0xa5});
    ReadCommittedCheckedUnsynced(range.Address() + Page - 128u, hole);
    for (std::size_t index = 0; index < hole.size(); ++index) Require(hole[index] == (index < 128u ? std::byte{0x5b} : std::byte{}), "a checked committed read did not preserve bytes around a guard page");
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(range.Data() + Page, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == (PAGE_READWRITE | PAGE_GUARD), "a checked committed read consumed a guard page");
    Require(VirtualProtect(range.Data() + Page, Page, previous, &previous) != 0, "cannot restore a checked-read guard page");
#endif
    bool rejected = false;
    try {
        ReadCommittedCheckedUnsynced(std::numeric_limits<std::uintptr_t>::max() - 7u, hole);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected, "an overflowing checked committed read did not throw");
#if defined(__linux__) || defined(__APPLE__)
    GuestArena::GuestArenaSetProtection_nid_postfix(range.Address() + Unit, Unit, 0x04);
#else
    GuestArena::GuestArenaCommit_nid_postfix(range.Data() + Unit, Unit, 0x04, Unit);
#endif
    std::memset(range.Data() + Unit, 0x6c, Unit);
    ReadCommittedCheckedUnsynced(range.Address() + Unit + 13u, hole);
    Require(std::all_of(hole.begin(), hole.end(), [](std::byte byte) { return byte == std::byte{0x6c}; }), "a checked committed read retained a reset page after restoring access");
    SetReadGuard([](std::uint64_t, std::size_t) { throw std::runtime_error("committed read guard"); });
    rejected = false;
    try {
        ReadCommittedCheckedUnsynced(range.Address(), hole);
    } catch (const std::runtime_error& error) {
        rejected = std::string_view(error.what()) == "committed read guard";
    }
    Require(rejected && std::all_of(hole.begin(), hole.end(), [](std::byte byte) { return byte == std::byte{0x6c}; }), "a checked committed read ignored its guard");
}

void Check(std::string_view mode) {
    if (mode == "committed") {
        CheckCommitted();
        return;
    }
    std::array<std::byte, 256> out{};
    if (mode == "boundaries") {
        Require(CopyMapped(0, {}) == Compare::Equal && CopyMapped(std::numeric_limits<std::uintptr_t>::max(), {}) == Compare::Equal, "an empty copy requires mapped memory");
        Require(CopyMapped(0, out) == Compare::Unmapped, "a null source was reported copied");
        Require(CopyMapped(std::numeric_limits<std::uintptr_t>::max() - 7u, out) == Compare::Unmapped, "an overflowing source range was reported copied");
        return;
    }
    Range range;
    Require(Accessible(range.Data(), 2 * Unit), "readable fixture cannot prime the access cache");
    if (mode == "hooks") {
        SetReadGuard(&NoteRead);
        SetFlushHook(&NoteFlush);
        const auto speculative = CopyMapped(range.Address(), out);
        const auto speculativeReads = readGuards;
        const auto speculativeFlushes = flushes;
        const bool ordinary = GuestCopyIn_nid_no_patch(out.data(), range.Data(), out.size());
        const auto ordinaryReads = readGuards;
        const auto ordinaryFlushes = flushes;
        SetReadGuard(nullptr);
        SetFlushHook(nullptr);
        Require(speculative == Compare::Equal, "a speculative copy with read hooks cannot read valid bytes");
        Require(speculativeReads == 0 && speculativeFlushes == 0, "a speculative mapped copy ran read guards or flushed GPU writes");
        Require(ordinary && ordinaryReads == 1 && ordinaryFlushes == 1, "an ordinary checked copy lost its read preparation");
        return;
    }
    if (mode == "reset") {
        GuestArena::GuestArenaReset_nid_postfix(range.Data(), 2 * Unit);
        errno = E2BIG;
        Require(CopyMapped(range.Address(), out) == Compare::Unmapped, "a reset source was reported copied through cached readable pages");
        Require(errno == E2BIG, "a failed speculative copy changed errno");
        return;
    }
    if (mode == "partial") {
        GuestArena::GuestArenaReset_nid_postfix(range.Data() + Unit, Unit);
        errno = E2BIG;
        Require(CopyMapped(range.Address() + Unit - 128u, out) == Compare::Unmapped, "a partly reset source was reported fully copied");
        Require(errno == E2BIG, "a partial speculative copy changed errno");
        return;
    }
    if (mode == "reuse") {
        GuestArena::GuestArenaReset_nid_postfix(range.Data(), 2 * Unit);
        GuestArena::GuestArenaCommit_nid_postfix(range.Data(), 2 * Unit, 0x04, Unit);
        std::memset(range.Data(), 0x6c, 2 * Unit);
        Require(CopyMapped(range.Address(), out) == Compare::Equal, "a recommitted source cannot be copied");
        Require(std::all_of(out.begin(), out.end(), [](std::byte byte) { return byte == std::byte{0x6c}; }), "a recommitted source returned old bytes");
        return;
    }
    Require(mode == "valid", "unknown mapped-copy fixture mode");
    errno = E2BIG;
    Require(CopyMapped(range.Address() + Unit - 128u, out) == Compare::Equal, "a readable source crossing an arena unit cannot be copied");
    Require(std::all_of(out.begin(), out.end(), [](std::byte byte) { return byte == std::byte{0x5b}; }), "a readable source returned different bytes");
    Require(errno == E2BIG, "a successful speculative copy changed errno");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "mapped-copy fixture requires a mode");
        Check(argv[1]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
