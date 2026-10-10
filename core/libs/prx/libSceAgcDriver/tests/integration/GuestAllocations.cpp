#include <Testing/Test.hpp>
#include "GraphicsTests.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Testing::Case;
using Testing::Require;

template<typename TAction>
void requireOwnershipRejection(const TAction& action, std::string_view what, std::source_location location = std::source_location::current()) {
    Testing::RequireThrows<std::runtime_error>(action, std::string("expected guest allocation ownership rejection: ") + std::string(what), location);
}

void releaseRangesOf(const void* allocation) {
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
        if (range->allocationAddress == reinterpret_cast<std::uintptr_t>(allocation)) ranges.emplace_back(range->address, range->bytes);
    }
    for (const auto& [address, bytes] : ranges) {
        try {
            GuestAllocations::Mutation mutation;
            mutation.Unmap(reinterpret_cast<const void*>(address), bytes, [](const void*, std::size_t, const void*, bool) {});
        } catch (const std::exception&) {
        }
    }
}

class GuestHeapBlock {
public:
    explicit GuestHeapBlock(void* pointer) : pointer(pointer) {}

    ~GuestHeapBlock() {
        if (pointer != nullptr) GuestHeap::GuestHeapFree_nid_postfix(pointer);
    }

    GuestHeapBlock(const GuestHeapBlock&) = delete;
    GuestHeapBlock& operator=(const GuestHeapBlock&) = delete;

    unsigned char* Bytes() const {
        return static_cast<unsigned char*>(pointer);
    }

    std::uintptr_t Address() const {
        return reinterpret_cast<std::uintptr_t>(pointer);
    }

    void Reallocate(std::size_t bytes) {
        pointer = GuestHeap::GuestHeapReallocate_nid_postfix(pointer, bytes);
    }

    void Free() {
        GuestHeap::GuestHeapFree_nid_postfix(pointer);
        pointer = nullptr;
    }

private:
    void* pointer;
};

const Case leaseWait{"Unmap_LeasedAllocation_WaitsForTheLeaseWhileOtherLookupsProceed", [] {
    std::array<std::byte, 128> memory{};
    const auto address = reinterpret_cast<std::uintptr_t>(memory.data());
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(memory.data(), 64, true, true);
        mutation.Add(memory.data() + 64, 64, true, true);
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
        } catch (...) {
            failure = std::current_exception();
        }
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
}};

const Case leasedHeapBlock{"GuestHeap_LeasedAllocation_RefusesFreeReallocAndProtection", [] {
    GuestHeapBlock block(GuestHeap::GuestHeapAllocate_nid_postfix(32));
    Require(block.Address() % alignof(std::max_align_t) == 0, "guest malloc is not suitably aligned");
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    Require(lease.size() == 1 && lease.front()->address == block.Address(), "guest heap registration is missing");
    requireOwnershipRejection([&] { GuestHeap::GuestHeapFree_nid_postfix(block.Bytes()); }, "a free of a leased block");
    requireOwnershipRejection([&] { GuestHeap::GuestHeapReallocate_nid_postfix(block.Bytes(), 64); }, "a realloc of a leased block");
    bool applied = false;
    {
        GuestAllocations::Mutation mutation;
        requireOwnershipRejection([&] { mutation.Protect(block.Bytes(), 32, true, false, [&] { applied = true; }); }, "a protection change of a leased block");
    }
    Require(!applied, "pinned guest protection changed");
    lease.clear();
    block.Free();
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "freed guest allocations remain registered");
}};

const Case heapReallocation{"GuestHeap_ReallocationAndAlignment_KeepDataAndAlignment", [] {
    {
        GuestHeapBlock block(GuestHeap::GuestHeapAllocate_nid_postfix(32));
        std::memset(block.Bytes(), 0x55, 32);
        block.Reallocate(64);
        for (std::size_t i = 0; i < 32; ++i) Require(block.Bytes()[i] == 0x55, "guest realloc lost data at byte " + std::to_string(i));
    }
    {
        GuestHeapBlock block(GuestHeap::GuestHeapAlign_nid_postfix(4, 32));
        Require(block.Address() % 4 == 0, "small guest alignment was not respected");
    }
    {
        GuestHeapBlock block(GuestHeap::GuestHeapAlign_nid_postfix(256, 32));
        Require(block.Address() % 256 == 0, "guest aligned allocation lost alignment");
        std::memset(block.Bytes(), 0x66, 32);
        block.Reallocate(64);
        Require(block.Bytes()[31] == 0x66, "aligned guest realloc lost data");
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "freed guest allocations remain registered");
}};

const Case partialMappings{"Mutation_PartialProtectionAndUnmaps_SplitAndReleaseTheMapping", [] {
    std::array<std::byte, 128> mapping{};
    struct Release {
        const void* allocation;
        ~Release() { releaseRangesOf(allocation); }
    } release{mapping.data()};
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(mapping.data(), mapping.size(), true, true);
        requireOwnershipRejection([&] { mutation.RequireAvailable(mapping.data() + 32, 16); }, "an occupied range reported as available");
        mutation.Protect(mapping.data() + 32, 32, true, false, [] {});
    }
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        Require(lease.size() == 3 && lease[0]->bytes == 32 && !lease[1]->writable && lease[2]->bytes == 64, "partial protection did not split the mapping");
        GuestAllocations::Mutation mutation;
        requireOwnershipRejection([&] { mutation.Unmap(mapping.data() + 32, 32, [](const void*, std::size_t, const void*, bool) {}); }, "an unmap of a leased range");
    }
    {
        GuestAllocations::Mutation mutation;
        bool applied = false;
        mutation.Unmap(mapping.data() + 32, 32, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "partial unmap released the allocation");
            applied = true;
        });
        Require(applied, "partial unmap callback was not called");
        requireOwnershipRejection([&] { mutation.Protect(mapping.data(), mapping.size(), true, true, [] {}); }, "a protection change over an unmapped hole");
        mutation.Unmap(mapping.data(), 32, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && !last, "first fragment released remaining mapping");
        });
        mutation.Unmap(mapping.data() + 64, 64, [&](const void*, std::size_t, const void* allocation, bool last) {
            Require(allocation == mapping.data() && last, "last fragment did not release the original allocation");
        });
    }
    Require(GuestAllocations::GuestAllocationsAcquire_nid_postfix().empty(), "unmapped fragments remain registered");
}};

#if defined(__linux__)
const Case unmappedGap{"CommittedRanges_UnmappedGapInHostMemory_DescribesThePagesAfterIt", [] {
    namespace GuestMemory = AgcDriver::GuestMemory;
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* mapped = mmap(nullptr, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED, "cannot map the gap test pages");
    auto* block = static_cast<std::byte*>(mapped);
    struct Unmap {
        std::byte* block;
        std::size_t page;
        ~Unmap() {
            munmap(block, page);
            munmap(block + 2 * page, page);
        }
    } unmap{block, page};
    Require(munmap(block + page, page) == 0, "cannot unmap the gap test's middle page");
    const auto base = reinterpret_cast<std::uint64_t>(block);
    const auto ranges = GuestMemory::CommittedRanges(base, 3 * page);
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> expected{{base, base + page}, {base + 2 * page, base + 3 * page}};
    Require(ranges == expected, "the pages after an unmapped gap in host memory were not described");
    Require(GuestMemory::CommittedRanges(base + page, 2 * page) == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{base + 2 * page, base + 3 * page}}, "the pages after a query that starts in a gap were not described");
    Require(!GuestMemory::Accessible(block, 3 * page) && GuestMemory::Accessible(block + 2 * page, page, true), "an unmapped gap in host memory is misreported");
    Require(!GuestMemory::Accessible(reinterpret_cast<const void*>(std::uintptr_t{0x18}), 4), "a near-null address counts as accessible");
}};
#endif

} // namespace

#ifdef _WIN32
void MainImageRegistrationCase() {
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
        allocationAddress = (*found)->allocationAddress;
        GuestAllocations::Mutation mutation;
        bool applied = false;
        requireOwnershipRejection([&] { mutation.Protect(&imageProbe, 1, true, false, [&] { applied = true; }); }, "a protection change of leased image memory");
        Require(!applied, "pinned image protection changed");
    }
    {
        GuestAllocations::Mutation mutation;
        requireOwnershipRejection([&] { mutation.Find(reinterpret_cast<void*>(allocationAddress)); }, "a lookup of the image allocation");
        requireOwnershipRejection([&] { mutation.Remove(reinterpret_cast<void*>(allocationAddress)); }, "a removal of the image allocation");
        bool applied = false;
        requireOwnershipRejection([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, std::size_t, const void*, bool) { applied = true; }); }, "an unmap of image memory");
        Require(!applied, "image memory was unmapped");
        requireOwnershipRejection([&] { mutation.Protect(&imageProbe, 1, true, false, [] { throw std::runtime_error("host protection failure"); }); }, "a protection change whose host protection fails");
    }
    Testing::RequireEqual(GuestAllocations::GuestAllocationsAcquire_nid_postfix().size(), imageRangeCount, "failed image protection changed registry ranges");
    GuestAllocations::Mutation mutation;
    mutation.Protect(&imageProbe, 1, true, true, [] {});
    bool applied = false;
    requireOwnershipRejection([&] { mutation.Unmap(&imageProbe, 1, [&](const void*, std::size_t, const void*, bool) { applied = true; }); }, "an unmap of split image memory");
    Require(!applied, "split image memory became releasable");
}
#endif
