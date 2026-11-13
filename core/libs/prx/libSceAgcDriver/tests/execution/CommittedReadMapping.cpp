#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
#ifdef _WIN32
void Run() {
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 4096;
    auto* source = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(source != nullptr, "cannot reserve snapshot fixture");
    GuestArena::GuestArenaCommit_nid_postfix(source, bytes, PAGE_READWRITE, bytes);
    std::memset(source, 0x5a, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    { GuestAllocations::Mutation mutation; mutation.Add(source, bytes, true, true, false); }
    std::array<std::byte, bytes> snapshot{};
    const auto prime = [&] {
        { GuestAllocations::Mutation mutation; mutation.Protect(source, bytes, true, true, false, [] {}); }
        Require(AgcDriver::GuestMemory::Accessible(source, bytes), "snapshot fixture is not readable");
    };
    const auto check = [&](bool hole) {
        for (std::size_t index = 0; index < bytes; ++index) {
            const auto expected = hole && index >= 2 * page && index < 3 * page ? std::byte{} : std::byte{0x5a};
            Require(snapshot[index] == expected, "committed snapshot lost readable bytes or copied inaccessible bytes");
        }
    };
    for (const DWORD protection : {PAGE_NOACCESS, PAGE_READWRITE | PAGE_GUARD}) {
        prime();
        DWORD previous = 0;
        Require(VirtualProtect(source + 2 * page, page, protection, &previous), "cannot protect snapshot hole");
        if ((protection & PAGE_GUARD) != 0) { GuestAllocations::Mutation mutation; mutation.Protect(source + 2 * page, page, true, true, false, [] {}); }
        snapshot.fill(std::byte{0xff});
        AgcDriver::GuestMemory::ReadCommitted(address, snapshot);
        check(true);
        MEMORY_BASIC_INFORMATION memory{};
        Require(VirtualQuery(source + 2 * page, &memory, sizeof(memory)) == sizeof(memory), "cannot query snapshot hole");
        Require(memory.Protect == protection, "snapshot read changed the hole's protection");
        Require(!AgcDriver::GuestMemory::Accessible(source, bytes), "failed snapshot retained stale readable page states");
        Require(VirtualProtect(source + 2 * page, page, PAGE_READWRITE, &previous), "cannot restore snapshot hole");
        prime();
        AgcDriver::GuestMemory::ReadCommittedUnsynced(address, snapshot);
        check(false);
        prime();
        Require(VirtualProtect(source + 2 * page, page, protection, &previous), "cannot protect comparison hole");
        if ((protection & PAGE_GUARD) != 0) { GuestAllocations::Mutation mutation; mutation.Protect(source + 2 * page, page, true, true, false, [] {}); }
        std::fill(snapshot.begin() + 2 * page, snapshot.begin() + 3 * page, std::byte{0x77});
        Require(AgcDriver::GuestMemory::EqualsCommitted(address, snapshot), "committed comparison did not skip an inaccessible page");
        snapshot[page - 1] = std::byte{};
        Require(!AgcDriver::GuestMemory::EqualsCommittedUnsynced(address, snapshot), "committed comparison missed a readable byte difference");
        Require(!AgcDriver::GuestMemory::Accessible(source, bytes), "failed comparison retained stale readable page states");
        Require(VirtualQuery(source + 2 * page, &memory, sizeof(memory)) == sizeof(memory), "cannot query comparison hole");
        Require(memory.Protect == protection, "committed comparison changed the hole's protection");
        Require(VirtualProtect(source + 2 * page, page, PAGE_READWRITE, &previous), "cannot restore comparison hole");
    }
    prime();
    std::atomic<bool> done{false};
    std::thread mutation([&] {
        DWORD previous = 0;
        while (!done.load(std::memory_order_acquire)) {
            VirtualProtect(source + 2 * page, page, PAGE_NOACCESS, &previous);
            VirtualProtect(source + 2 * page, page, PAGE_READWRITE, &previous);
        }
    });
    bool stable = true;
    try {
        for (unsigned iteration = 0; iteration < 500; ++iteration) {
            AgcDriver::GuestMemory::ReadCommittedUnsynced(address, snapshot);
            stable = stable && std::all_of(snapshot.begin(), snapshot.begin() + 2 * page, [](auto value) { return value == std::byte{0x5a}; });
            stable = stable && std::all_of(snapshot.begin() + 3 * page, snapshot.end(), [](auto value) { return value == std::byte{0x5a}; });
        }
    } catch (...) {
        done.store(true, std::memory_order_release);
        mutation.join();
        throw;
    }
    done.store(true, std::memory_order_release);
    mutation.join();
    Require(stable, "concurrent snapshot discarded stable readable pages");
    { GuestAllocations::Mutation mutation; mutation.Remove(source); }
    GuestArena::GuestArenaReset_nid_postfix(source, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(source, bytes);
}

void RunReleasedPages() {
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 4096;
    constexpr std::size_t middle = 5 * page;
    constexpr std::size_t last = bytes - page;
    auto* source = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(source != nullptr, "cannot reserve released page fixture");
    GuestArena::GuestArenaCommit_nid_postfix(source, bytes, PAGE_READWRITE, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    { GuestAllocations::Mutation mutation; mutation.Add(source, bytes, true, true, false); }
    const auto release = [&](std::size_t offset) { Require(VirtualFree(source + offset, page, MEM_DECOMMIT), "cannot release fixture page"); };
    const auto restore = [&](std::size_t offset) { Require(VirtualAlloc(source + offset, page, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot restore fixture page"); };
    std::vector<std::byte> pattern(bytes);
    for (std::size_t index = 0; index < bytes; ++index) pattern[index] = static_cast<std::byte>(index * 31 >> 3);
    const auto prepare = [&] {
        restore(middle);
        restore(last);
        std::memcpy(source, pattern.data(), bytes);
        { GuestAllocations::Mutation mutation; mutation.Protect(source, bytes, true, true, false, [] {}); }
        Require(AgcDriver::GuestMemory::Accessible(source, bytes), "released page fixture is not readable");
        release(middle);
        release(last);
    };
    const auto hole = [&](std::size_t index) { return (index >= middle && index < middle + page) || index >= last; };

    prepare();
    std::vector<std::byte> copy(bytes, std::byte{0xff});
    Require(!GuestArena::GuestArenaCopyOwned_nid_no_patch(address, copy.data(), bytes), "direct copy did not report a released page");
    bool equal = true;
    Require(!GuestArena::GuestArenaCompareOwned_nid_no_patch(address, pattern.data(), bytes, &equal), "direct comparison did not report a released page");
    std::fill(copy.begin(), copy.end(), std::byte{0xff});
    AgcDriver::GuestMemory::ReadCommitted(address, copy);
    for (std::size_t index = 0; index < bytes; ++index) Require(copy[index] == (hole(index) ? std::byte{} : pattern[index]), "released pages were not zero in a copy, or readable pages were lost");
    Require(!AgcDriver::GuestMemory::Accessible(source, bytes), "a released page kept its readable state");

    prepare();
    auto expected = pattern;
    for (std::size_t index = 0; index < bytes; ++index) {
        if (hole(index)) expected[index] = std::byte{0x77};
    }
    Require(AgcDriver::GuestMemory::EqualsCommittedUnsynced(address, expected), "comparison did not leave out released pages");
    for (const std::size_t mismatch : {page - 1, middle - 1, middle + page, last - 1}) {
        prepare();
        expected[mismatch] ^= std::byte{1};
        Require(!AgcDriver::GuestMemory::EqualsCommittedUnsynced(address, expected), "comparison missed a difference beside a released page");
        expected[mismatch] ^= std::byte{1};
    }

    for (std::size_t before = 1; before < 130; before += 3) {
        for (std::size_t after = 1; after < 40; after += 7) {
            prepare();
            std::vector<std::byte> straddle(before + after, std::byte{0xff});
            AgcDriver::GuestMemory::ReadCommitted(address + middle - before, straddle);
            for (std::size_t index = 0; index < before; ++index) Require(straddle[index] == pattern[middle - before + index], "straddling copy lost bytes before a released page");
            for (std::size_t index = before; index < straddle.size(); ++index) Require(straddle[index] == std::byte{}, "straddling copy did not zero a released page");
            prepare();
            std::vector<std::byte> straddleExpected(pattern.data() + middle - before, pattern.data() + middle + after);
            std::fill(straddleExpected.begin() + before, straddleExpected.end(), std::byte{0x77});
            Require(AgcDriver::GuestMemory::EqualsCommittedUnsynced(address + middle - before, straddleExpected), "straddling comparison did not leave out a released page");
        }
    }

    prepare();
    for (std::size_t length = 1; length < 80; ++length) {
        std::vector<std::byte> edge(length);
        const auto first = address + middle - length;
        Require(GuestArena::GuestArenaCopyOwned_nid_no_patch(first, edge.data(), length), "direct copy read past the end of its range");
        Require(std::memcmp(edge.data(), pattern.data() + middle - length, length) == 0, "direct copy returned wrong bytes");
        bool same = false;
        Require(GuestArena::GuestArenaCompareOwned_nid_no_patch(first, edge.data(), length, &same) && same, "direct comparison read past the end of its range");
        edge[length - 1] ^= std::byte{1};
        Require(GuestArena::GuestArenaCompareOwned_nid_no_patch(first, edge.data(), length, &same) && !same, "direct comparison missed a last-byte difference");
    }

    auto* host = static_cast<std::byte*>(VirtualAlloc(nullptr, 3 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(host != nullptr, "cannot allocate host pages");
    std::memset(host, 0x5a, 3 * page);
    DWORD previous = 0;
    Require(VirtualProtect(host + page, page, PAGE_READWRITE | PAGE_GUARD, &previous), "cannot guard host page");
    std::vector<std::byte> outside(3 * page, std::byte{0xff});
    Require(!GuestArena::GuestArenaCopyOwned_nid_no_patch(reinterpret_cast<std::uintptr_t>(host), outside.data(), outside.size()), "host pages were read directly");
    AgcDriver::GuestMemory::ReadCommitted(reinterpret_cast<std::uintptr_t>(host), outside);
    for (std::size_t index = 0; index < outside.size(); ++index) Require(outside[index] == (index >= page && index < 2 * page ? std::byte{} : std::byte{0x5a}), "host guard page read changed");
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(host + page, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == (PAGE_READWRITE | PAGE_GUARD), "host guard page protection changed");
    Require(VirtualFree(host, 0, MEM_RELEASE), "cannot release host pages");

    prepare();
    restore(last);
    std::memcpy(source + last, pattern.data() + last, page);
    expected = pattern;
    std::fill(expected.begin() + middle, expected.begin() + middle + page, std::byte{});
    std::atomic<bool> done{false};
    std::thread mutation([&] {
        while (!done.load(std::memory_order_acquire)) {
            VirtualFree(source + middle, page, MEM_DECOMMIT);
            VirtualAlloc(source + middle, page, MEM_COMMIT, PAGE_READWRITE);
        }
    });
    bool stable = true;
    try {
        for (unsigned iteration = 0; iteration < 2000; ++iteration) {
            AgcDriver::GuestMemory::ReadCommittedUnsynced(address, copy);
            stable = stable && std::memcmp(copy.data(), pattern.data(), middle) == 0 && std::memcmp(copy.data() + middle + page, pattern.data() + middle + page, bytes - middle - page) == 0;
            stable = stable && AgcDriver::GuestMemory::EqualsCommittedUnsynced(address, expected);
        }
    } catch (...) {
        done.store(true, std::memory_order_release);
        mutation.join();
        throw;
    }
    done.store(true, std::memory_order_release);
    mutation.join();
    Require(stable, "concurrent decommit changed the result of a read");
    restore(middle);
    { GuestAllocations::Mutation mutation; mutation.Remove(source); }
    GuestArena::GuestArenaReset_nid_postfix(source, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(source, bytes);
}
#endif
}

int main() {
    try {
#ifdef _WIN32
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run();
        RunReleasedPages();
        device->WaitIdle();
        std::puts("committed snapshot mapping changes passed");
        return 0;
#else
        return VulkanTestSkipped;
#endif
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
