#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

namespace {
using namespace AgcDriver::GuestMemory;
constexpr std::size_t Unit = 65536;
constexpr std::size_t ChunkGuestBytes = 256ull << 20u;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Range {
public:
    explicit Range(std::size_t bytes) : bytes(bytes) {
        data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, ChunkGuestBytes));
        Require(data != nullptr, "cannot reserve page-state fixture");
    }

    ~Range() {
        {
            GuestAllocations::Mutation mutation;
            for (const auto offset : committed) mutation.Remove(data + offset);
        }
        GuestArena::GuestArenaReset_nid_postfix(data, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
    }

    std::byte* Commit(std::size_t offset, std::size_t length = Unit) {
        auto* at = data + offset;
        GuestArena::GuestArenaCommit_nid_postfix(at, length, PAGE_READWRITE, Unit);
        std::memset(at, 0x5b, length);
        GuestAllocations::Mutation mutation;
        mutation.Add(at, length, true, true, true);
        committed.push_back(offset);
        return at;
    }

    std::byte* Data() const { return data; }

private:
    std::byte* data = nullptr;
    std::size_t bytes;
    std::vector<std::size_t> committed;
};

std::size_t PrivateBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    Require(K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)), "cannot query page-state fixture private commit");
    return counters.PrivateUsage;
}

void CheckMetadata() {
    Range range(3 * ChunkGuestBytes);
    auto* data = range.Commit(0);
    const auto before = PrivateBytes();
    Require(Accessible(data, Unit, true), "committed fixture is inaccessible");
    const auto after = PrivateBytes();
    const auto added = after > before ? after - before : 0;
    std::printf("page-permission initialization: %zu bytes of private commit\n", added);
    Require(added < (16ull << 20u), "page permissions committed a dense arena table");
    const auto queried = PrivateBytes();
    Require(!Accessible(range.Data() + 2 * ChunkGuestBytes, Unit), "reserved fixture tail is accessible");
    Require(PrivateBytes() < queried + (4ull << 20u), "querying reserved pages allocated untouched metadata chunks");
}

void CheckBoundaries() {
    constexpr auto span = 3 * ChunkGuestBytes;
    Range range(span);
    const std::array<std::size_t, 4> offsets{0, ChunkGuestBytes - Unit, ChunkGuestBytes, 2 * ChunkGuestBytes + Unit};
    for (const auto offset : offsets) range.Commit(offset);
    auto* block = range.Data();
    std::atomic<bool> begin{false};
    std::atomic<unsigned> errors{0};
    std::vector<std::thread> readers;
    for (unsigned reader = 0; reader < 8; ++reader) {
        readers.emplace_back([&] {
            while (!begin.load(std::memory_order_acquire)) std::this_thread::yield();
            for (unsigned round = 0; round < 500; ++round) {
                if (!Accessible(block + offsets.back(), Unit, true)) errors.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    begin.store(true, std::memory_order_release);
    for (auto& reader : readers) reader.join();
    Require(errors.load() == 0, "concurrent page-state publication changed access");
    for (const auto offset : offsets) {
        std::array<std::byte, 16> copy{};
        Read(reinterpret_cast<std::uint64_t>(block + offset), copy);
        Require(std::all_of(copy.begin(), copy.end(), [](std::byte value) { return value == std::byte{0x5b}; }), "page cache changed guest bytes");
    }
    Require(Accessible(block + ChunkGuestBytes - Unit, 2 * Unit, true), "adjacent chunks lost their boundary pages");
    const auto described = DescribeCommitted(reinterpret_cast<std::uint64_t>(block), span, true);
    std::uint64_t total = 0;
    for (const auto& [first, last] : described.ranges) total += last - first;
    Require(!described.whole && total == offsets.size() * Unit, "reserved holes were reported committed");
    auto* changed = block + offsets[1];
    const auto protect = [&](bool writable) {
        GuestAllocations::Mutation mutation;
        mutation.Protect(changed, Unit, true, writable, true, [&] {
            const DWORD protection = writable ? PAGE_READWRITE : PAGE_READONLY;
            DWORD previous = 0;
            Require(VirtualProtect(changed, Unit, protection, &previous) != 0, "cannot protect page-state fixture");
            GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(changed), Unit, protection);
        });
    };
    protect(false);
    Require(Accessible(changed, Unit) && !Accessible(changed, Unit, true), "protection invalidation retained writable state");
    protect(true);
    Require(Accessible(changed, Unit, true), "restored writable state was not discovered");
    GuestArena::GuestArenaReset_nid_postfix(block, Unit);
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block), Unit);
    Require(!Accessible(block, Unit), "decommitted pages remained readable");
    GuestArena::GuestArenaCommit_nid_postfix(block, Unit, PAGE_READWRITE, Unit);
    Require(Accessible(block, Unit, true), "access observer did not invalidate inaccessible state after commit");
    std::puts("page-state boundaries, publication, holes and invalidation passed");
}

void CheckRuns() {
    constexpr std::size_t page = 4096;
    constexpr std::size_t count = 1536;
    constexpr auto bytes = count * page;
    using Ranges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
    Range range(3 * ChunkGuestBytes);
    auto* guest = range.Commit(ChunkGuestBytes - 512 * page, bytes);
    const auto base = reinterpret_cast<std::uint64_t>(guest);
    const auto expected = [&](std::size_t index, std::size_t length = 1) {
        Ranges result;
        if (index != 0) result.emplace_back(base, base + index * page);
        if (index + length < count) result.emplace_back(base + (index + length) * page, base + bytes);
        return result;
    };
    const auto protect = [&](std::size_t index, bool writable) {
        auto* at = guest + index * page;
        GuestAllocations::Mutation mutation;
        mutation.Protect(at, page, true, writable, true, [&] {
            const DWORD protection = writable ? PAGE_READWRITE : PAGE_READONLY;
            DWORD previous = 0;
            Require(VirtualProtect(at, page, protection, &previous) != 0, "cannot protect run fixture page");
            GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(at), page, protection);
        });
    };
    const Ranges whole{{base, base + bytes}};
    Require(DescribeCommitted(base, bytes, true).whole, "run fixture was not fully writable");
    for (const std::size_t index : {0, 63, 64, 511, 512, 513, 700, 1023, 1024, 1535}) {
        protect(index, false);
        Require(!Accessible(guest, bytes, true) && Accessible(guest, bytes), "a protected page retained writable access in a summarized run");
        Require(CommittedRanges(base, bytes, true) == expected(index), "a block or chunk boundary retained a stale summary");
        Require(CommittedRanges(base, bytes) == whole, "readable ranges changed around a read-only page");
        Require(CommittedRanges(base + 1, bytes - 2, true) == [&] {
            auto clipped = expected(index);
            if (!clipped.empty() && clipped.front().first == base) ++clipped.front().first;
            if (!clipped.empty() && clipped.back().second == base + bytes) --clipped.back().second;
            return clipped;
        }(), "an unaligned run crossed a protected page");
        protect(index, true);
        Require(DescribeCommitted(base, bytes, true).whole, "restored run was not writable");
    }
    auto* hole = guest + 512 * page;
    GuestArena::GuestArenaReset_nid_postfix(hole, Unit);
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(hole), Unit);
    Require(!Accessible(hole, page), "a decommitted page retained cached access");
    Require(CommittedRanges(base, bytes) == expected(512, Unit / page), "decommitted pages retained a block summary");
    GuestArena::GuestArenaCommit_nid_postfix(hole, Unit, PAGE_READWRITE, Unit);
    Require(DescribeCommitted(base, bytes, true).whole, "recommit did not clear inaccessible state and its summary");
    std::atomic<bool> done{false};
    std::atomic<std::size_t> epoch{0};
    std::atomic<std::size_t> changed{0};
    std::array<std::atomic<std::size_t>, 2> observed{};
    std::atomic<unsigned> errors{0};
    std::vector<std::thread> readers;
    for (std::size_t reader = 0; reader < observed.size(); ++reader) {
        readers.emplace_back([&, reader] {
            while (!done.load(std::memory_order_acquire)) {
                const auto phase = epoch.load(std::memory_order_acquire);
                if (phase > observed[reader].load(std::memory_order_relaxed)) {
                    const auto index = changed.load(std::memory_order_relaxed);
                    if (CommittedRanges(base, bytes, true) != ((phase & 1) != 0 ? expected(index) : whole)) errors.fetch_add(1, std::memory_order_relaxed);
                    observed[reader].store(phase, std::memory_order_release);
                } else static_cast<void>(DescribeCommitted(base, bytes, true));
            }
        });
    }
    const auto publish = [&](std::size_t phase, std::size_t index) {
        changed.store(index, std::memory_order_relaxed);
        epoch.store(phase, std::memory_order_release);
        for (auto& seen : observed) {
            while (seen.load(std::memory_order_acquire) < phase) std::this_thread::yield();
        }
    };
    for (std::size_t round = 0; round < 128; ++round) {
        const auto index = (round * 37 + 511) % count;
        protect(index, false);
        publish(2 * round + 1, index);
        protect(index, true);
        publish(2 * round + 2, index);
    }
    for (std::size_t round = 0; round < 2000; ++round) {
        const auto index = 448 + round % 128;
        protect(index, false);
        protect(index, true);
    }
    protect(512, false);
    publish(257, 512);
    done.store(true, std::memory_order_release);
    for (auto& reader : readers) reader.join();
    Require(errors.load() == 0 && CommittedRanges(base, bytes, true) == expected(512), "concurrent scans retained a stale block summary");
    protect(512, true);
    std::array<std::byte, 16> copy{};
    Read(base, copy);
    Require(std::all_of(copy.begin(), copy.end(), [](std::byte value) { return value == std::byte{0x5b}; }), "summary scans changed guest data");
    Range edge(2 * ChunkGuestBytes);
    constexpr std::size_t edgePages = 512;
    const auto edgeFirst = reinterpret_cast<std::uint64_t>(edge.Commit(ChunkGuestBytes - edgePages * page, edgePages * page));
    const Ranges edgeRun{{edgeFirst, edgeFirst + edgePages * page}};
    Require(CommittedRanges(edgeFirst, edgePages * page, true) == edgeRun, "the run before an unallocated chunk was not writable");
    Require(!DescribeCommitted(edgeFirst, edgePages * page + Unit, true).whole && CommittedRanges(edgeFirst, edgePages * page + Unit, true) == edgeRun, "a run into an unallocated chunk was reported past its committed pages");
    std::puts("page-state block scans across 64-page, 512-page and sparse-chunk boundaries passed");
}
}

int main(int argc, char** argv) {
    try {
        Require(GuestArena::GuestArenaAvailable_nid_postfix(), "page-state fixture needs the guest arena");
        if (argc == 2 && std::string_view(argv[1]) == "--metadata") CheckMetadata();
        else if (argc == 2 && std::string_view(argv[1]) == "--runs") CheckRuns();
        else CheckBoundaries();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
