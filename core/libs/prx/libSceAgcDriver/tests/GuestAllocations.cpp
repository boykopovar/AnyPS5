#include "BdaTests.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include <cstring>
#include <atomic>
#include <exception>
#include <thread>
#include <array>
#include <algorithm>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

template<typename TAction>
void reject(TAction action) {
    try { action(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("expected guest allocation ownership rejection");
}

}

void RunGuestLeaseWaitTests() {
    std::array<std::byte, 128> memory{};
    const auto address = reinterpret_cast<std::uintptr_t>(memory.data());
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory.data(), 64, true, true, true);
        mutation.Add(memory.data() + 64, 64, true, true, true);
    }
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    std::erase_if(lease, [&](const auto& range) { return range->address != address; });
    std::atomic<bool> mutationEntered = false;
    std::atomic<bool> lookupFinished = false;
    std::exception_ptr failure;
    bool unmapped = false;
    std::jthread mutator([&] {
        try {
            GuestAllocations::Mutation mutation;
            mutationEntered.store(true);
            mutationEntered.notify_one();
            mutation.Unmap(memory.data(), 64, [&](const void*, std::size_t, const void*, bool) {
                Require(lookupFinished.load(), "the allocation was unmapped before its lease holder finished");
                unmapped = true;
            });
        } catch (...) { failure = std::current_exception(); }
    });
    mutationEntered.wait(false);
    auto other = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    std::erase_if(other, [&](const auto& range) { return range->address != address + 64; });
    const bool found = other.size() == 1 && other.front()->address == address + 64;
    lookupFinished.store(true);
    lease.clear();
    mutator.join();
    other.clear();
    {
        GuestAllocations::Mutation mutation;
        if (!unmapped) mutation.Remove(memory.data());
        mutation.Remove(memory.data() + 64);
    }
    Require(found, "a lease holder could not query another allocation during an unmap wait");
    if (failure) std::rethrow_exception(failure);
    Require(unmapped, "waiting unmap did not resume after the lease was released");
}

void RunGuestAllocationTests() {
    void* pointer = GuestHeap::GuestHeapAllocate_nid_postfix(32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % alignof(std::max_align_t) == 0, "guest malloc is not suitably aligned");
    std::memset(pointer, 0x55, 32);
    {
        auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        Require(lease.size() == 1 && lease.front()->address == reinterpret_cast<std::uintptr_t>(pointer), "guest heap registration is missing");
        reject([&] { GuestHeap::GuestHeapFree_nid_postfix(pointer); });
        reject([&] { GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64); });
        GuestAllocations::Mutation mutation;
        bool applied = false;
        reject([&] { mutation.Protect(pointer, 32, true, false, true, [&] { applied = true; }); });
        Require(!applied, "pinned guest protection changed");
    }
    pointer = GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64);
    for (std::size_t i = 0; i < 32; ++i) Require(static_cast<unsigned char*>(pointer)[i] == 0x55, "guest realloc lost data");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    pointer = GuestHeap::GuestHeapAlign_nid_postfix(4, 32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % 4 == 0, "small guest alignment was not respected");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    pointer = GuestHeap::GuestHeapAlign_nid_postfix(256, 32);
    Require(reinterpret_cast<std::uintptr_t>(pointer) % 256 == 0, "guest aligned allocation lost alignment");
    std::memset(pointer, 0x66, 32);
    pointer = GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 64);
    Require(static_cast<unsigned char*>(pointer)[31] == 0x66, "aligned guest realloc lost data");
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "freed guest allocations remain registered");
    std::array<std::byte, 128> mapping{};
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(mapping.data(), mapping.size(), true, true, true);
        reject([&] { mutation.RequireAvailable(mapping.data() + 32, 16); });
        mutation.Protect(mapping.data() + 32, 32, true, false, true, [] {});
    }
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        Require(lease.size() == 3 && lease[0]->bytes == 32 && !lease[1]->writable && lease[2]->bytes == 64, "partial protection did not split the mapping");
        GuestAllocations::Mutation mutation;
        reject([&] { mutation.Unmap(mapping.data() + 32, 32, [](const void*, std::size_t, const void*, bool) {}); });
    }
    {
        GuestAllocations::Mutation mutation;
        bool applied = false;
        mutation.Unmap(mapping.data() + 32, 32, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "partial unmap released the allocation");
            applied = true;
        });
        Require(applied, "partial unmap callback was not called");
        reject([&] { mutation.Protect(mapping.data(), mapping.size(), true, true, true, [] {}); });
        mutation.Unmap(mapping.data(), 32, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "first fragment released remaining mapping");
        });
        mutation.Unmap(mapping.data() + 64, 64, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && last, "last fragment did not release the original allocation");
        });
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "unmapped fragments remain registered");
#ifdef _WIN32
    static std::byte imageProbe{};
    {
        GuestAllocations::Mutation mutation;
        mutation.RegisterMainImage();
        mutation.RegisterMainImage();
    }
    const auto imageAddress = reinterpret_cast<std::uintptr_t>(&imageProbe);
    std::uint64_t allocationAddress = 0;
    std::size_t imageRangeCount = 0;
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        imageRangeCount = lease.size();
        const auto found = std::find_if(lease.begin(), lease.end(), [&](const auto& range) { return imageAddress >= range->address && imageAddress - range->address < range->bytes; });
        Require(found != lease.end() && (*found)->writable && !(*found)->releasable, "main image registration is missing or releasable");
        Require(std::none_of(lease.begin(), lease.end(), [](const auto& range) { return !range->releasable && range->gpu; }), "main image registered as GPU-mapped");
        allocationAddress = (*found)->allocationAddress;
        GuestAllocations::Mutation mutation;
        bool applied = false;
        reject([&] { mutation.Protect(&imageProbe, 1, true, false, true, [&] { applied = true; }); });
        Require(!applied, "pinned image protection changed");
    }
    {
        GuestAllocations::Mutation mutation;
        reject([&] { mutation.Find(reinterpret_cast<void*>(allocationAddress)); });
        reject([&] { mutation.Remove(reinterpret_cast<void*>(allocationAddress)); });
        bool applied = false;
        reject([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, std::size_t, const void*, bool) { applied = true; }); });
        Require(!applied, "image memory was unmapped");
        reject([&] { mutation.Protect(&imageProbe, 1, true, false, true, [] { throw std::runtime_error("host protection failure"); }); });
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().size() == imageRangeCount, "failed image protection changed registry ranges");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(&imageProbe, 1, true, true, true, [] {});
        bool applied = false;
        reject([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, std::size_t, const void*, bool) { applied = true; }); });
        Require(!applied, "split image memory became releasable");
    }
#endif
}

void RunUnmappedGapTests() {
#if defined(__linux__)
    namespace GuestMemory = AgcDriver::GuestMemory;
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* block = static_cast<std::byte*>(mmap(nullptr, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(block != MAP_FAILED, "cannot map the gap test pages");
    Require(munmap(block + page, page) == 0, "cannot unmap the gap test's middle page");
    const auto base = reinterpret_cast<std::uint64_t>(block);
    const auto ranges = GuestMemory::CommittedRanges(base, 3 * page);
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> expected{{base, base + page}, {base + 2 * page, base + 3 * page}};
    Require(ranges == expected, "the pages after an unmapped gap in host memory were not described");
    Require(GuestMemory::CommittedRanges(base + page, 2 * page) == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{base + 2 * page, base + 3 * page}}, "the pages after a query that starts in a gap were not described");
    Require(!GuestMemory::Accessible(block, 3 * page) && GuestMemory::Accessible(block + 2 * page, page, true), "an unmapped gap in host memory is misreported");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(std::uintptr_t{0x18}), 4), "a near-null address counts as accessible");
    munmap(block, page);
    munmap(block + 2 * page, page);
    if (!GuestArena::GuestArenaAvailable_nid_postfix()) return;
    auto* guest = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * page, page));
    Require(mmap(guest, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == guest, "cannot map the arena page test pages");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(guest, 2 * page, true, true, true);
    }
    Require(GuestMemory::Accessible(guest, page, true), "a mapped arena page is not writable");
    Require(mprotect(guest, page, PROT_NONE) == 0, "cannot revoke the arena test page behind the registry");
    Require(GuestMemory::Accessible(guest, page, true), "a checked arena page was queried again instead of read from the page table");
    Require(mprotect(guest, page, PROT_READ | PROT_WRITE) == 0, "cannot restore the arena test page behind the registry");
    Require(mprotect(guest + page, page, PROT_READ) == 0, "cannot protect the arena test's second page");
    Require(!GuestMemory::Accessible(guest + page, page, true), "a check stored arena pages outside its range");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(guest + page, page, true, true, true, [&] { Require(mprotect(guest + page, page, PROT_READ | PROT_WRITE) == 0, "cannot restore the arena test's second page"); });
    }
    Require(GuestMemory::Accessible(guest, 2 * page, true), "mapped arena pages are not writable");
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(guest, page, true, false, true, [&] { Require(mprotect(guest, page, PROT_READ) == 0, "cannot protect the arena test page"); });
    }
    Require(!GuestMemory::Accessible(guest, page, true) && GuestMemory::Accessible(guest, page) && GuestMemory::Accessible(guest + page, page, true), "an arena page protected through the registry kept its old access");
    {
        GuestAllocations::Mutation mutation;
        const auto release = [](const void* pointer, std::size_t bytes, const void*, bool) { Require(munmap(const_cast<void*>(pointer), bytes) == 0, "cannot unmap an arena test page"); };
        mutation.Unmap(guest, page, release);
        mutation.Unmap(guest + page, page, release);
    }
    Require(!GuestMemory::Accessible(guest, page) && !GuestMemory::Accessible(guest + page, page), "unmapped arena pages are still accessible");
    GuestArena::GuestArenaRelease_nid_postfix(guest, 2 * page);
#endif
}
