#include "prx/libc/include/GuestWriteWatch.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__linux__)
#error The read trap tests cover the Linux write watch
#endif

using namespace GuestWriteWatch;

namespace {

constexpr int Skipped = 77;
constexpr std::uint8_t Stored = 0xab;

std::atomic<unsigned> resolved{0};
std::atomic<std::intptr_t> aliasOffset{0};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Resolve(std::uintptr_t begin, std::uintptr_t end) {
    std::memset(reinterpret_cast<void*>(begin + aliasOffset.load(std::memory_order_acquire)), Stored, end - begin);
    resolved.fetch_add(1, std::memory_order_acq_rel);
}

int Protection(const void* page) {
    const auto address = reinterpret_cast<std::uintptr_t>(page);
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        char* parsed = nullptr;
        const auto first = static_cast<std::uintptr_t>(std::strtoull(line.c_str(), &parsed, 16));
        if (parsed == nullptr || *parsed != '-') continue;
        const auto last = static_cast<std::uintptr_t>(std::strtoull(parsed + 1, &parsed, 16));
        if (address < first || address >= last || parsed == nullptr || std::strlen(parsed) < 4) continue;
        return (parsed[1] == 'r' ? PROT_READ : 0) | (parsed[2] == 'w' ? PROT_WRITE : 0);
    }
    throw std::runtime_error("the page is not mapped");
}

struct Region {
    explicit Region(std::size_t pageCount, int protection = PROT_READ | PROT_WRITE) : page(static_cast<std::size_t>(::getpagesize())), bytes(pageCount * page) {
        void* mapped = mmap(nullptr, bytes, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "cannot map the region");
        base = static_cast<std::uint8_t*>(mapped);
        if ((protection & PROT_WRITE) != 0) std::memset(base, 0x11, bytes);
        GuestWriteWatchRegister_nid_postfix(base, bytes);
        Require(GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(base), bytes), "the region is not watched");
    }

    ~Region() {
        GuestWriteWatchUnregister_nid_postfix(base, bytes);
        munmap(base, bytes);
    }

    std::uintptr_t Address(std::size_t pageIndex = 0) const {
        return reinterpret_cast<std::uintptr_t>(base) + pageIndex * page;
    }

    std::uint8_t ReadFromThread(std::size_t offset) const {
        std::uint8_t value = 0;
        std::thread reader([&] { value = static_cast<volatile std::uint8_t*>(base)[offset]; });
        reader.join();
        return value;
    }

    std::size_t page;
    std::size_t bytes;
    std::uint8_t* base = nullptr;
};

void CheckTrap() {
    Region region(4);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), 2 * region.page), "a read-write range was not trapped");
    Require(Protection(region.base) == PROT_NONE && Protection(region.base + region.page) == PROT_NONE, "a trapped page stayed accessible");
    Require(Protection(region.base + 2 * region.page) == (PROT_READ | PROT_WRITE), "a trap took a page outside its range");
    Require(region.ReadFromThread(region.page + 5) == Stored, "a read of a trapped page did not see the resolver's store");
    Require(resolved.load() == 1, "the resolver did not run exactly once");
    Require(Protection(region.base) == (PROT_READ | PROT_WRITE) && Protection(region.base + region.page) == (PROT_READ | PROT_WRITE), "a resolved trap did not give back its pages");
    Require(region.base[0] == Stored, "the resolver did not store the whole range");
    region.base[0] = 0x22;
    Require(region.base[0] == 0x22, "a resolved page is not writable");
}

void CheckRelease() {
    Region region(2);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-write page was not trapped");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(), region.page);
    Require(Protection(region.base) == (PROT_READ | PROT_WRITE), "a released trap kept its page");
    Require(region.base[3] == 0x11 && resolved.load() == 0, "releasing a trap ran the resolver");
}

void CheckProtect() {
    Region region(2);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-write page was not trapped");
    Require(GuestWriteWatchProtect_nid_postfix(region.base, region.page, PROT_READ) == 0, "protecting a trapped page failed");
    Require(resolved.load() == 1 && Protection(region.base) == PROT_READ, "protecting a trapped page did not resolve it first");
    Require(region.base[0] == Stored, "the protected page lost the resolver's store");
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-only page was trapped");
    Require(GuestWriteWatchProtect_nid_postfix(region.base, region.page, PROT_READ | PROT_WRITE) == 0, "making the page writable again failed");
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a page made read-write again was not trapped");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(), region.page);
}

void CheckHostWrite() {
    Region region(2);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-write page was not trapped");
    GuestWriteWatchBeginHostWrite_nid_postfix(region.base, 16);
    Require(resolved.load() == 1 && Protection(region.base) == (PROT_READ | PROT_WRITE), "a host write over a trapped page did not resolve it first");
    region.base[0] = 0x33;
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a page under a host write was trapped");
    GuestWriteWatchEndHostWrite_nid_postfix(region.base, 16);
    Require(region.base[0] == 0x33, "the host write was lost");
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a page was not trapped after its host write ended");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(), region.page);
}

std::mutex holderLock;
std::atomic<bool> resolving{false};
thread_local bool holding = false;

void ResolveHeld(std::uintptr_t begin, std::uintptr_t end) {
    resolving.store(true, std::memory_order_release);
    const std::lock_guard held(holderLock);
    Resolve(begin, end);
}

void CheckHolder() {
    Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveHeld), "cannot set the read trap resolver");
    GuestWriteWatchSetReadTrapBypass_nid_postfix(+[] { return holding; });
    Region region(1);
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-write page was not trapped");
    std::unique_lock gate(holderLock);
    holding = true;
    std::uint8_t seen = 0;
    std::thread reader([&] { seen = static_cast<volatile std::uint8_t*>(region.base)[0]; });
    while (!resolving.load(std::memory_order_acquire)) std::this_thread::yield();
    Require(static_cast<volatile std::uint8_t*>(region.base)[1] == 0x11, "a read by the holder of the resolver's lock saw bytes the resolver had not stored");
    holding = false;
    gate.unlock();
    reader.join();
    Require(seen == Stored, "a read of a trapped page did not wait for the resolver");
    Require(resolved.load() == 1, "the trap was not resolved exactly once");
}

void CheckTrapped() {
    Region region(4);
    std::size_t trapped = 0;
    const auto count = [&] {
        trapped = 0;
        GuestWriteWatchReadTraps_nid_postfix(region.Address(), region.bytes, +[](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &trapped);
        return trapped;
    };
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(1), 2 * region.page), "a read-write range was not trapped");
    Require(count() == 2 * region.page, "the trapped range was not reported");
    Require(region.ReadFromThread(region.page) == Stored, "a read of a trapped page did not see the resolver's store");
    Require(count() == 0, "a resolved range was still reported as trapped");
}

void CheckUnknown() {
    Region region(2, PROT_READ);
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(), region.page), "a read-only mapping was trapped");
    Require(Protection(region.base) == PROT_READ, "a refused trap changed the page");
}

void CheckUnregister() {
    const auto page = static_cast<std::size_t>(::getpagesize());
    void* mapped = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "cannot map the region");
    std::memset(mapped, 0x44, 2 * page);
    GuestWriteWatchRegister_nid_postfix(mapped, 2 * page);
    Require(GuestWriteWatchTrapReads_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page), "a read-write page was not trapped");
    GuestWriteWatchUnregister_nid_postfix(mapped, 2 * page);
    Require(Protection(mapped) == (PROT_READ | PROT_WRITE), "unregistering a trapped page did not give it back");
    Require(static_cast<std::uint8_t*>(mapped)[0] == 0x44 && resolved.load() == 0, "unregistering a trapped page ran the resolver");
    munmap(mapped, 2 * page);
}

void CheckThreads() {
    constexpr unsigned Rounds = 200;
    constexpr unsigned Readers = 8;
    const auto page = static_cast<std::size_t>(::getpagesize());
    const auto bytes = Readers * page;
    const int file = memfd_create("aps5-read-trap-threads", MFD_CLOEXEC);
    Require(file >= 0 && ftruncate(file, static_cast<off_t>(bytes)) == 0, "cannot create the shared backing");
    auto* base = static_cast<std::uint8_t*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
    auto* alias = static_cast<std::uint8_t*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
    close(file);
    Require(base != MAP_FAILED && alias != MAP_FAILED, "cannot map the two views of the shared backing");
    aliasOffset.store(alias - base, std::memory_order_release);
    GuestWriteWatchRegister_nid_postfix(base, bytes);
    Require(GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(base), bytes), "the region is not watched");
    for (unsigned round = 0; round < Rounds; ++round) {
        std::memset(base, 0x11, bytes);
        Require(GuestWriteWatchTrapReads_nid_postfix(reinterpret_cast<std::uintptr_t>(base), bytes), "a read-write range was not trapped");
        std::atomic<bool> start{false};
        std::atomic<unsigned> stale{0};
        std::vector<std::thread> readers;
        for (unsigned reader = 0; reader < Readers; ++reader) {
            readers.emplace_back([&, reader] {
                while (!start.load()) std::this_thread::yield();
                const auto* view = static_cast<volatile std::uint8_t*>(base);
                if (view[reader * page + reader] != Stored) stale.fetch_add(1);
            });
        }
        start.store(true);
        for (auto& reader : readers) reader.join();
        Require(stale.load() == 0, "a concurrent read of a trapped range did not see the resolver's store");
    }
    Require(resolved.load() == Rounds, "concurrent reads did not resolve each trap exactly once");
    aliasOffset.store(0, std::memory_order_release);
    GuestWriteWatchUnregister_nid_postfix(base, bytes);
    munmap(alias, bytes);
    munmap(base, bytes);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected trap, release, protect, host, unknown, unregister, threads, holder or trapped");
        if (!GuestWriteWatchAvailable_nid_postfix()) {
            std::puts("the userfaultfd write watch is not available");
            return Skipped;
        }
        const std::string mode = argv[1];
        if (mode == "holder") {
            CheckHolder();
            return 0;
        }
        Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&Resolve), "cannot set the read trap resolver");
        if (mode == "trap") CheckTrap();
        else if (mode == "release") CheckRelease();
        else if (mode == "protect") CheckProtect();
        else if (mode == "host") CheckHostWrite();
        else if (mode == "unknown") CheckUnknown();
        else if (mode == "unregister") CheckUnregister();
        else if (mode == "threads") CheckThreads();
        else if (mode == "trapped") CheckTrapped();
        else throw std::runtime_error("unknown mode");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
