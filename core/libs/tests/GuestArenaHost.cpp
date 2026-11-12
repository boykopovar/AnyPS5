#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#if !defined(__APPLE__)
#error These tests cover the macOS guest arena reservation
#endif

using namespace GuestArena;

namespace {

constexpr std::uintptr_t ArenaStart = 0x0000000200000000ull;
constexpr std::uintptr_t SystemReservedStart = 0x00000007FFFFC000ull;
constexpr std::uintptr_t SystemReservedEnd = 0x0000001000000000ull;
constexpr std::uintptr_t ApplicationAreaEnd = 0x000000FC00000000ull;
constexpr std::uintptr_t ReservationChunk = 1ull << 30;
constexpr std::size_t GapBytes = 0x10000;
constexpr std::uint32_t ProtectionNoAccess = 0x01;
constexpr std::uint32_t ProtectionReadOnly = 0x02;
constexpr std::uint32_t ProtectionReadWrite = 0x04;

struct HostRegion {
    std::uintptr_t start;
    std::uintptr_t end;
    vm_prot_t maxProtection;
};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<HostRegion> RegionsInArenaRange() {
    std::vector<HostRegion> regions;
    mach_vm_address_t address = ArenaStart;
    while (address < ApplicationAreaEnd) {
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) break;
        if (address >= ApplicationAreaEnd) break;
        regions.push_back({static_cast<std::uintptr_t>(address), static_cast<std::uintptr_t>(address + size), info.max_protection});
        address += size;
    }
    return regions;
}

const std::vector<HostRegion> RegionsBeforeArena = RegionsInArenaRange();

bool Overlaps(const std::vector<HostRegion>& regions, std::uintptr_t start, std::uintptr_t end) {
    for (const auto& region : regions) {
        if (region.start < end && start < region.end) return true;
    }
    return false;
}

void CheckAvailable() {
    Require(GuestArenaAvailable_nid_postfix(), "the guest arena is unavailable on macOS");
    std::uintptr_t base = 0;
    std::size_t bytes = 0;
    GuestArenaRange_nid_postfix(&base, &bytes);
    Require(base == ArenaStart, "the guest arena does not start at the PS5 application map base");
    Require(base + bytes == ApplicationAreaEnd, "the guest arena does not end at the PS5 application map limit");
}

void CheckSkipsUnmappable() {
    for (const auto& region : RegionsInArenaRange()) {
        if (region.maxProtection != VM_PROT_NONE || region.end - region.start < 0x10000) continue;
        void* placed = GuestArenaAllocateAtOrAbove_nid_postfix(region.start, 0x10000, 0x10000);
        const auto address = reinterpret_cast<std::uintptr_t>(placed);
        Require(address >= region.end, "an allocation landed inside a range the kernel never lets the process map");
        GuestArenaRelease_nid_postfix(placed, 0x10000);
        return;
    }
    std::puts("no unmappable range inside the arena on this host; nothing to skip");
}

void CheckHostRegionsKept() {
    std::size_t hostRegions = 0;
    for (const auto& region : RegionsInArenaRange()) {
        if (region.maxProtection == VM_PROT_ALL) continue;
        ++hostRegions;
        bool rejected = false;
        try {
            GuestArenaMarkUsed_nid_postfix(reinterpret_cast<void*>(region.start), region.end - region.start);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected, "a fixed guest mapping over a host region was accepted");
    }
    std::printf("%zu host regions inside the arena range are protected\n", hostRegions);
}

void CheckGapBesideHostRegion() {
    Require(GuestArenaAvailable_nid_postfix(), "the guest arena is unavailable on macOS");
    Require(RegionsBeforeArena.size() < RegionsInArenaRange().size(), "the host map was read after the guest arena reserved its range");
    for (const auto& host : RegionsBeforeArena) {
        const auto chunk = host.start & ~(ReservationChunk - 1);
        for (auto gap = std::max(chunk, ArenaStart); gap + GapBytes <= chunk + ReservationChunk && gap + GapBytes <= ApplicationAreaEnd; gap += GapBytes) {
            if (Overlaps(RegionsBeforeArena, gap, gap + GapBytes) || (gap < SystemReservedEnd && SystemReservedStart < gap + GapBytes)) continue;
            auto* pointer = reinterpret_cast<void*>(gap);
            GuestArenaMarkUsed_nid_postfix(pointer, GapBytes);
            GuestArenaRelease_nid_postfix(pointer, GapBytes);
            std::printf("free range 0x%llx beside host region 0x%llx-0x%llx accepted\n", static_cast<unsigned long long>(gap),
                static_cast<unsigned long long>(host.start), static_cast<unsigned long long>(host.end));
            return;
        }
    }
    std::puts("no free range shares a 1 GiB reservation chunk with a host region on this host; nothing to check");
}

void CheckCommitAndProtect() {
    constexpr std::size_t bytes = 0x10000;
    auto* pointer = static_cast<std::uint8_t*>(GuestArenaAllocate_nid_postfix(bytes, bytes));
    GuestArenaCommit_nid_postfix(pointer, bytes, ProtectionReadWrite, bytes);
    pointer[0] = 0x5a;
    pointer[bytes - 1] = 0xa5;
    Require(pointer[0] == 0x5a && pointer[bytes - 1] == 0xa5, "committed guest memory does not keep writes");
    GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes, ProtectionReadOnly);
    Require(pointer[0] == 0x5a, "read-only guest memory lost its contents");
    GuestArenaReset_nid_postfix(pointer, bytes);
    GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes, ProtectionNoAccess);
    GuestArenaRelease_nid_postfix(pointer, bytes);
}

struct WatchedMapping {
    WatchedMapping() {
        Require(GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix(), "write watch is unavailable");
        pointer = static_cast<std::uint8_t*>(GuestArenaAllocate_nid_postfix(bytes, page));
        GuestArenaCommit_nid_postfix(pointer, bytes, ProtectionReadWrite, page);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pointer, bytes);
    }
    ~WatchedMapping() {
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pointer, bytes);
        GuestArenaReset_nid_postfix(pointer, bytes);
        GuestArenaRelease_nid_postfix(pointer, bytes);
    }
    std::set<std::uintptr_t> Collect() {
        std::set<std::uintptr_t> pages;
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes,
            [](void* context, std::uintptr_t begin, std::uintptr_t end) {
                auto& output = *static_cast<std::set<std::uintptr_t>*>(context);
                for (auto address = begin; address < end; address += ::getpagesize()) output.insert(address);
            }, &pages), "cannot collect watched arena pages");
        return pages;
    }
    const std::size_t page = static_cast<std::size_t>(::getpagesize());
    const std::size_t bytes = 3 * page;
    std::uint8_t* pointer = nullptr;
};

void CheckWatchedProtection() {
    WatchedMapping mapping;
    const auto middle = reinterpret_cast<std::uintptr_t>(mapping.pointer + mapping.page);
    const std::set<std::uintptr_t> changed{middle};
    Require(mapping.Collect().size() == 3 && mapping.Collect().empty(), "cannot arm arena pages");
    GuestArenaSetProtection_nid_postfix(middle, mapping.page, ProtectionReadWrite);
    mapping.pointer[mapping.page] = 42;
    Require(mapping.Collect() == changed, "arena protection change lost a write");
    Require(mapping.Collect().empty(), "arena write was reported twice");
    for (const auto protection : {ProtectionReadOnly, ProtectionNoAccess}) {
        GuestArenaSetProtection_nid_postfix(middle, mapping.page, protection);
        std::uintptr_t end = 0;
        const auto recorded = GuestWriteWatch::GuestWriteWatchProtection_nid_postfix(middle, middle + mapping.page, &end);
        Require(recorded == (protection == ProtectionReadOnly ? PROT_READ : PROT_NONE) && end == middle + mapping.page,
            "arena protection change left stale watch permissions");
        Require(mapping.Collect() == changed && mapping.Collect().empty(), "protection change touched the wrong pages");
        GuestArenaSetProtection_nid_postfix(middle, mapping.page, ProtectionReadWrite);
        mapping.pointer[mapping.page] = 43;
        Require(mapping.Collect() == changed && mapping.Collect().empty(), "restored write was not tracked");
    }
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapping.pointer, mapping.bytes);
    Require(mapping.pointer[mapping.page] == 43, "arena protection change lost stored data");
}

void CheckWatchedProtectionFaults() {
    for (const auto protection : {ProtectionReadOnly, ProtectionNoAccess}) {
        for (const bool armed : {false, true}) {
            for (const bool hostWrite : {false, true}) {
                const auto child = ::fork();
                Require(child >= 0, "cannot fork arena protection test");
                if (child == 0) {
                    ::alarm(2);
                    try {
                        WatchedMapping mapping;
                        if (armed) mapping.Collect();
                        if (hostWrite) GuestWriteWatch::GuestWriteWatchBeginHostWrite_nid_postfix(mapping.pointer, mapping.bytes);
                        GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping.pointer), mapping.bytes, protection);
                        *static_cast<volatile std::uint8_t*>(mapping.pointer) = 42;
                    } catch (...) {
                        ::_exit(2);
                    }
                    ::_exit(0);
                }
                int status = 0;
                Require(::waitpid(child, &status, 0) == child, "cannot wait for arena protection test");
                Require(WIFSIGNALED(status) && (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV),
                    "write watch bypassed arena protection or retried the fault");
            }
        }
    }
}

void CheckFailedWatchedProtection() {
    WatchedMapping mapping;
    mapping.Collect();
    const auto address = reinterpret_cast<std::uintptr_t>(mapping.pointer);
    bool rejected = false;
    try {
        GuestArenaSetProtection_nid_postfix(address + 1, mapping.page, ProtectionNoAccess);
    } catch (const std::system_error& error) {
        rejected = error.code() == std::errc::invalid_argument;
    }
    Require(rejected, "unaligned protection change was not rejected with EINVAL");
    std::uintptr_t end = 0;
    Require(GuestWriteWatch::GuestWriteWatchProtection_nid_postfix(address, address + mapping.page, &end) == (PROT_READ | PROT_WRITE),
        "failed protection change modified watch permissions");
    mapping.pointer[0] = 42;
    Require(mapping.Collect() == std::set<std::uintptr_t>{address}, "failed protection change lost write tracking");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected a guest arena test mode");
        const std::string_view mode(argv[1]);
        if (mode == "available") CheckAvailable();
        else if (mode == "unmappable") CheckSkipsUnmappable();
        else if (mode == "host") CheckHostRegionsKept();
        else if (mode == "gap") CheckGapBesideHostRegion();
        else if (mode == "commit") CheckCommitAndProtect();
        else if (mode == "watch-protection") CheckWatchedProtection();
        else if (mode == "watch-faults") CheckWatchedProtectionFaults();
        else if (mode == "watch-failure") CheckFailedWatchedProtection();
        else throw std::runtime_error("unknown guest arena test");
        std::printf("guest arena %s passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
