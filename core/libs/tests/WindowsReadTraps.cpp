#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

using namespace GuestWriteWatch;

namespace {

constexpr std::size_t Page = 4096;
constexpr std::size_t Block = 65536;
constexpr std::uint8_t Stored = 0xab;

std::atomic<unsigned> resolved{0};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Resolve(std::uintptr_t begin, std::uintptr_t end) {
    std::memset(reinterpret_cast<void*>(begin), Stored, end - begin);
    resolved.fetch_add(1, std::memory_order_acq_rel);
}

DWORD Protection(const void* page) {
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(page, &memory, sizeof(memory)) == sizeof(memory), "cannot query the page");
    return memory.State == MEM_COMMIT ? memory.Protect : 0;
}

struct Region {
    explicit Region(std::size_t bytes, DWORD protection = PAGE_READWRITE) : bytes(bytes) {
        base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block));
        Require(base != nullptr, "cannot reserve guest memory");
        GuestArena::GuestArenaCommit_nid_postfix(base, bytes, PAGE_READWRITE, bytes);
        std::memset(base, 0x11, bytes);
        if (protection != PAGE_READWRITE) {
            DWORD previous = 0;
            Require(VirtualProtect(base, bytes, protection, &previous) != 0, "cannot protect the region");
        }
    }

    ~Region() {
        if (base == nullptr) return;
        GuestArena::GuestArenaReset_nid_postfix(base, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(base, bytes);
    }

    std::uintptr_t Address(std::size_t page = 0) const {
        return reinterpret_cast<std::uintptr_t>(base) + page * Page;
    }

    std::uint8_t ReadFromThread(std::size_t offset) const {
        std::uint8_t value = 0;
        std::thread reader([&] { value = static_cast<volatile std::uint8_t*>(base)[offset]; });
        reader.join();
        return value;
    }

    std::size_t bytes;
    std::uint8_t* base = nullptr;
};

void CheckTrap() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), 2 * Page), "a read-write range was not trapped");
    Require(Protection(region.base) == PAGE_NOACCESS && Protection(region.base + Page) == PAGE_NOACCESS, "a trapped page stayed accessible");
    Require(Protection(region.base + 2 * Page) == PAGE_READWRITE, "a trap took a page outside its range");
    Require(region.ReadFromThread(Page + 5) == Stored, "a read of a trapped page did not see the resolver's store");
    Require(resolved.load() == 1, "the resolver did not run exactly once");
    Require(Protection(region.base) == PAGE_READWRITE && Protection(region.base + Page) == PAGE_READWRITE, "a resolved trap did not give back its pages");
    Require(region.base[0] == Stored, "the resolver did not store the whole range");
    region.base[0] = 0x22;
    Require(region.base[0] == 0x22, "a resolved page is not writable");
}

void CheckRelease() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a read-write page was not trapped");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(), Page);
    Require(Protection(region.base) == PAGE_READWRITE, "a released trap kept its page");
    Require(region.base[3] == 0x11 && resolved.load() == 0, "releasing a trap ran the resolver");
}

void CheckHostWrite() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a read-write page was not trapped");
    GuestWriteWatchBeginHostWrite_nid_postfix(region.base, 16);
    Require(resolved.load() == 1 && Protection(region.base) == PAGE_READWRITE, "a host write over a trapped page did not resolve it first");
    region.base[0] = 0x33;
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a page under a host write was trapped");
    GuestWriteWatchEndHostWrite_nid_postfix(region.base, 16);
    Require(region.base[0] == 0x33, "the host write was lost");
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a page was not trapped after its host write ended");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(), Page);
}

void CheckUnknown() {
    Region region(Block, PAGE_READONLY);
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a read-only page was trapped");
    Require(Protection(region.base) == PAGE_READONLY, "a refused trap changed the page");
    auto* host = VirtualAlloc(nullptr, Page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(host != nullptr, "cannot allocate host memory");
    Require(!GuestWriteWatchTrapReads_nid_postfix(reinterpret_cast<std::uintptr_t>(host), Page), "memory outside the guest arena was trapped");
    Require(Protection(host) == PAGE_READWRITE, "a refused trap changed host memory");
    VirtualFree(host, 0, MEM_RELEASE);
}

void CheckReset() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), Page), "a read-write page was not trapped");
    GuestArena::GuestArenaReset_nid_postfix(region.base, region.bytes);
    Require(resolved.load() == 1, "resetting a trapped range did not resolve its trap first");
    GuestArena::GuestArenaRelease_nid_postfix(region.base, region.bytes);
    region.base = nullptr;
}

void CheckTrapped() {
    Region region(Block);
    std::size_t trapped = 0;
    const auto count = [&] {
        trapped = 0;
        GuestWriteWatchReadTraps_nid_postfix(region.Address(), region.bytes, +[](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &trapped);
        return trapped;
    };
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), 2 * Page), "a read-write range was not trapped");
    Require(count() == 2 * Page, "the trapped range was not reported");
    Require(region.ReadFromThread(Page) == Stored, "a read of a trapped page did not see the resolver's store");
    Require(count() == 0, "a resolved range was still reported as trapped");
}

void CheckGeneration() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), Page), "a read-write page was not trapped");
    const auto held = GuestWriteWatchReadTrapGeneration_nid_postfix();
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), Page) && GuestWriteWatchReadTrapGeneration_nid_postfix() == held, "setting a held trap again changed the generation");
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(3), Page) && GuestWriteWatchReadTrapGeneration_nid_postfix() == held, "setting another trap changed the generation");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(3), Page);
    const auto released = GuestWriteWatchReadTrapGeneration_nid_postfix();
    Require(released != held, "releasing a trap kept the generation");
    Require(region.ReadFromThread(Page) == Stored, "a read of a trapped page did not see the resolver's store");
    Require(GuestWriteWatchReadTrapGeneration_nid_postfix() != released, "resolving a trap kept the generation");
}

void CheckUntrapped() {
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), Page), "a read-write page was not trapped");
    Require(!GuestWriteWatchBeginUntrappedAccess_nid_postfix(region.base, 2 * Page), "an untrapped access was granted over a trapped page");
    Require(resolved.load() == 0 && Protection(region.base + Page) == PAGE_NOACCESS, "a refused untrapped access resolved the trap");
    Require(GuestWriteWatchBeginUntrappedAccess_nid_postfix(region.base + 2 * Page, 2 * Page), "an untrapped access was refused over pages no trap covers");
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(3), Page), "a page under an untrapped access was trapped");
    GuestWriteWatchEndUntrappedAccess_nid_postfix(region.base + 2 * Page, 2 * Page);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(3), Page), "a page was not trapped after its untrapped access ended");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(3), Page);
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(1), Page);
    Require(resolved.load() == 0, "untrapped accesses ran the resolver");
}

void CheckAlias() {
    const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(Block), nullptr);
    Require(section != nullptr, "cannot create the direct memory backing");
    auto* base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(Block, Block));
    Require(base != nullptr, "cannot reserve guest memory");
    GuestArena::GuestArenaMapDirect_nid_postfix(base, Block, section, 0, PAGE_READWRITE);
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    Require(GuestArena::GuestArenaResidentSpan_nid_postfix(address, Block, &start, &end) && start == address && end == address + Block, "direct memory is not one resident run");
    std::memset(base, 0x11, Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(address + Page, Page), "a resident read-write page was not trapped");
    const auto held = GuestWriteWatchReadTrapGeneration_nid_postfix();
    auto* alias = GuestArena::GuestArenaMapAlias_nid_postfix(address, Block);
    Require(alias != nullptr, "cannot alias the resident run");
    Require(GuestWriteWatchReadTrapGeneration_nid_postfix() != held, "an alias that demotes a resident run kept the generation of the trap in it");
    Require(base[Page] == 0x11 && resolved.load() == 0, "an alias that demotes a resident run resolved the trap in it");
    GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    GuestArena::GuestArenaReset_nid_postfix(base, Block);
    GuestArena::GuestArenaRelease_nid_postfix(base, Block);
    GuestArena::GuestArenaForgetSection_nid_postfix(section);
    CloseHandle(section);
}

std::atomic<bool> resolving{false};
SRWLOCK holderLock = SRWLOCK_INIT;
thread_local bool holding = false;

void ResolveHeld(std::uintptr_t begin, std::uintptr_t end) {
    resolving.store(true, std::memory_order_release);
    AcquireSRWLockExclusive(&holderLock);
    Resolve(begin, end);
    ReleaseSRWLockExclusive(&holderLock);
}

void CheckHolder() {
    Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveHeld), "cannot set the read trap resolver");
    GuestWriteWatchSetReadTrapBypass_nid_postfix(+[] { return holding; });
    Region region(Block);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), Page), "a read-write page was not trapped");
    AcquireSRWLockExclusive(&holderLock);
    holding = true;
    std::uint8_t seen = 0;
    std::thread reader([&] { seen = static_cast<volatile std::uint8_t*>(region.base)[0]; });
    while (!resolving.load(std::memory_order_acquire)) std::this_thread::yield();
    Require(static_cast<volatile std::uint8_t*>(region.base)[1] == 0x11, "a read by the holder of the resolver's lock saw bytes the resolver had not stored");
    holding = false;
    ReleaseSRWLockExclusive(&holderLock);
    reader.join();
    Require(seen == Stored, "a read of a trapped page did not wait for the resolver");
    Require(resolved.load() == 1, "the trap was not resolved exactly once");
}

void CheckThreads() {
    constexpr unsigned Rounds = 200;
    constexpr unsigned Readers = 8;
    Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(+[](std::uintptr_t, std::uintptr_t) { resolved.fetch_add(1, std::memory_order_acq_rel); }), "cannot set the read trap resolver");
    Region region(Block);
    for (unsigned round = 0; round < Rounds; ++round) {
        std::memset(region.base, 0x11, Readers * Page);
        Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), Readers * Page), "a read-write range was not trapped");
        std::atomic<bool> start{false};
        std::atomic<unsigned> early{0};
        std::vector<std::thread> readers;
        for (unsigned reader = 0; reader < Readers; ++reader) {
            readers.emplace_back([&, reader] {
                while (!start.load()) std::this_thread::yield();
                static_cast<void>(static_cast<volatile std::uint8_t*>(region.base)[reader * Page + reader]);
                if (resolved.load(std::memory_order_acquire) <= round) early.fetch_add(1);
            });
        }
        start.store(true);
        for (auto& reader : readers) reader.join();
        Require(early.load() == 0, "a concurrent read of a trapped range went on before the resolver returned");
    }
    Require(resolved.load() == Rounds, "concurrent reads did not resolve each trap exactly once");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected trap, release, host, unknown, reset, alias, trapped, generation, untrapped, holder or threads");
        const std::string mode = argv[1];
        if (mode == "holder") {
            CheckHolder();
            return 0;
        }
        if (mode == "threads") {
            CheckThreads();
            return 0;
        }
        Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&Resolve), "cannot set the read trap resolver");
        if (mode == "trap") CheckTrap();
        else if (mode == "release") CheckRelease();
        else if (mode == "host") CheckHostWrite();
        else if (mode == "unknown") CheckUnknown();
        else if (mode == "reset") CheckReset();
        else if (mode == "alias") CheckAlias();
        else if (mode == "trapped") CheckTrapped();
        else if (mode == "generation") CheckGeneration();
        else if (mode == "untrapped") CheckUntrapped();
        else throw std::runtime_error("unknown mode");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
