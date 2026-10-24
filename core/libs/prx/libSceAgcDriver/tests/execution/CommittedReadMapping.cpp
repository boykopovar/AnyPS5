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
        snapshot.fill(std::byte{0xff});
        AgcDriver::GuestMemory::ReadCommitted(address, snapshot);
        check(true);
        MEMORY_BASIC_INFORMATION memory{};
        Require(VirtualQuery(source + 2 * page, &memory, sizeof(memory)) == sizeof(memory), "cannot query snapshot hole");
        Require(memory.Protect == protection, "snapshot read changed the hole's protection");
        Require(VirtualProtect(source + 2 * page, page, PAGE_READWRITE, &previous), "cannot restore snapshot hole");
        prime();
        AgcDriver::GuestMemory::ReadCommittedUnsynced(address, snapshot);
        check(false);
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
#endif
}

int main() {
    try {
#ifdef _WIN32
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run();
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
