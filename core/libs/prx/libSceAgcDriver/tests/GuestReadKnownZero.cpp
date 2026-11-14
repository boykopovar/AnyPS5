#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// #18281: on Windows, write-watch reports a read that faults in a demand-zero page as a write. The
// driver's read primitives must leave never-written pages unreported, while a guest store (also a
// zero store, #18144) made after or during the read is still collected.

#ifdef _WIN32
namespace {

using namespace AgcDriver::GuestMemory;

constexpr std::size_t Page = 4096;
constexpr std::size_t Block = 65536;
constexpr std::size_t Bytes = 8u << 20u;
constexpr std::size_t Mib = 1u << 20u;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Arena {
    std::byte* memory = nullptr;
    Arena() {
        memory = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(Bytes, Block));
        Require(memory != nullptr, "the guest arena allocation failed");
        Commit();
    }
    ~Arena() {
        GuestArena::GuestArenaReset_nid_postfix(memory, Bytes);
        GuestArena::GuestArenaRelease_nid_postfix(memory, Bytes);
    }
    void Commit() const {
        GuestArena::GuestArenaCommit_nid_postfix(memory, Bytes, PAGE_READWRITE, Bytes);
    }
    void Recommit() const {
        GuestArena::GuestArenaReset_nid_postfix(memory, Bytes);
        Commit();
    }
    std::uint64_t At(std::size_t offset) const {
        return reinterpret_cast<std::uint64_t>(memory) + offset;
    }
    void Store(std::size_t offset, std::uint8_t value) const {
        reinterpret_cast<volatile std::uint8_t*>(memory)[offset] = value;
    }
};

bool Zero(std::span<const std::byte> bytes) {
    return std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
}

void CheckReadsLeaveNeverWrittenPagesUnreported(const Arena& arena) {
    const auto before = CollectWritesUncached(arena.At(0), Bytes);
    Require(before != 0, "the first collect failed");
    std::vector<std::byte> copy(Bytes, std::byte{0x5a});
    CopyGuestBytes(copy.data(), arena.memory, 2 * Mib);
    Require(Zero(std::span(copy).first(2 * Mib)), "CopyGuestBytes over never-written pages did not return zeros");
    Read(arena.At(2 * Mib), std::span(copy).subspan(2 * Mib, Mib));
    ReadCommitted(arena.At(3 * Mib), std::span(copy).subspan(3 * Mib, Mib));
    Require(Zero(std::span(copy).first(4 * Mib)), "Read or ReadCommitted over never-written pages did not return zeros");
    const std::vector<std::byte> zeros(2 * Mib);
    std::vector<std::byte> nonzero(Mib);
    nonzero[Mib / 2 + 3] = std::byte{1};
    Require(EqualsCommitted(arena.At(4 * Mib), std::span(zeros).first(Mib)), "EqualsCommitted over never-written pages did not equal zeros");
    Require(!EqualsCommitted(arena.At(5 * Mib), nonzero), "EqualsCommitted over never-written pages equalled a nonzero byte");
    Require(CompareMapped(arena.At(6 * Mib), std::span(zeros).first(Mib)) == Compare::Equal, "CompareMapped over never-written pages did not equal zeros");
    Require(CompareMapped(arena.At(7 * Mib), nonzero) == Compare::Differs, "CompareMapped over never-written pages did not differ from a nonzero byte");
    const auto after = CollectWritesUncached(arena.At(0), Bytes);
    Require(after != 0, "the collect after the reads failed");
    Require(UnchangedSince(arena.At(0), Bytes, before), "the driver's own reads of never-written pages were collected as guest writes");

    arena.Store(5 * Page + 17, 0);
    arena.Store(40 * Block + 9, 7);
    Require(CollectWritesUncached(arena.At(0), Bytes) != 0, "the collect after the stores failed");
    Require(WrittenSince(arena.At(5 * Page), Page, after), "a guest zero store after a driver read was not collected");
    Require(WrittenSince(arena.At(40 * Block), Page, after), "a guest store after a driver read was not collected");
    Require(UnchangedSince(arena.At(Block), 39 * Block, after) && UnchangedSince(arena.At(41 * Block), Bytes - 41 * Block, after), "pages no store reached were collected as written");
    CopyGuestBytes(copy.data(), arena.memory, Bytes);
    Require(copy[40 * Block + 9] == std::byte{7}, "a read after a guest store did not return the stored byte");
}

void CheckStoresRacingAReadAreReported(Arena& arena) {
    std::vector<std::byte> copy(2 * Mib);
    for (std::size_t round = 0; round < 200; ++round) {
        arena.Recommit();
        const auto zeroPage = (round * 7) % (2 * Mib / Page);
        const auto valuePage = (zeroPage + 2 * Mib / Page / 2) % (2 * Mib / Page);
        const auto before = CollectWritesUncached(arena.At(0), 2 * Mib);
        Require(before != 0, "the collect before the race failed");
        std::atomic<bool> ready{false};
        std::atomic<bool> go{false};
        std::thread guest([&] {
            ready.store(true);
            while (!go.load()) std::this_thread::yield();
            arena.Store(zeroPage * Page + 64, 0);
            arena.Store(valuePage * Page + 64, 7);
        });
        while (!ready.load()) std::this_thread::yield();
        go.store(true);
        CopyGuestBytes(copy.data(), arena.memory, copy.size());
        guest.join();
        const auto value = copy[valuePage * Page + 64];
        Require(value == std::byte{0} || value == std::byte{7}, "a read racing a guest store returned a byte the guest never held");
        Require(CollectWritesUncached(arena.At(0), 2 * Mib) != 0, "the collect after the race failed");
        Require(WrittenSince(arena.At(zeroPage * Page), Page, before), "a guest zero store racing a driver read was not collected (round " + std::to_string(round) + ")");
        Require(WrittenSince(arena.At(valuePage * Page), Page, before), "a guest store racing a driver read was not collected (round " + std::to_string(round) + ")");
    }
}

}
#endif

int main() {
#ifdef _WIN32
    try {
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::cout << "guest read known-zero test skipped: the guest arena is not write watched\n";
            return 77;
        }
        Arena arena;
        CheckReadsLeaveNeverWrittenPagesUnreported(arena);
        CheckStoresRacingAReadAreReported(arena);
        std::cout << "guest read known-zero tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
#else
    std::cout << "guest read known-zero test skipped: only Windows write-watch reports a read as a write\n";
    return 77;
#endif
}
