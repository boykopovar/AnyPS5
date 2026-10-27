#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTALLOCATIONS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTALLOCATIONS_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <mutex>
#include <vector>

namespace GuestAllocations {

struct Range {
    std::uint64_t address;
    std::size_t bytes;
    bool readable;
    bool writable;
    std::uint64_t allocationAddress;
    std::size_t allocationBytes;
    bool releasable = true;
    bool gpu = false;
    std::uint64_t backingSerial = 0;
};

class Lease {
public:
    using Ranges = std::vector<std::shared_ptr<const Range>>;
    using value_type = Ranges::value_type;
    using const_iterator = Ranges::const_iterator;
    using iterator = const_iterator;

    Lease() = default;
    explicit Lease(std::shared_ptr<const Ranges> ranges) : ranges(std::move(ranges)) {}

    const_iterator begin() const { return held().begin(); }
    const_iterator end() const { return held().end(); }
    std::size_t size() const { return held().size(); }
    bool empty() const { return held().empty(); }
    const value_type& operator[](std::size_t index) const { return held()[index]; }
    const value_type& front() const { return held().front(); }
    const value_type& back() const { return held().back(); }
    void clear() { ranges.reset(); }
    const std::shared_ptr<const Ranges>& Snapshot() const { return ranges; }

private:
    const Ranges& held() const {
        static const Ranges none;
        return ranges != nullptr ? *ranges : none;
    }

    std::shared_ptr<const Ranges> ranges;
};
using Mapped = std::vector<std::weak_ptr<const Range>>;

extern "C" {
void* GuestAllocationsBegin_nid_postfix();
void GuestAllocationsRegisterMainImage_nid_postfix(void* mutation);
void GuestAllocationsRegisterImage_nid_postfix(void* mutation, const void* image);
void GuestAllocationsUnregisterImage_nid_postfix(void* mutation, const void* image);
void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept;
void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu);
void GuestAllocationsRequireUnpinned_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
void GuestAllocationsRequireAvailable_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
bool GuestAllocationsCovers_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
bool GuestAllocationsOverlaps_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
bool GuestAllocationsForRegistered_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t)>& visit);
Range GuestAllocationsFind_nid_postfix(void* mutation, const void* pointer);
void GuestAllocationsRemove_nid_postfix(void* mutation, const void* pointer);
void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply);
void GuestAllocationsUnmap_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t, const void*, bool)>& apply);
Lease GuestAllocationsAcquire_nid_postfix();
Lease GuestAllocationsAcquireMapped_nid_no_patch(const Mapped& mapped);
Lease GuestAllocationsAcquireAll_nid_postfix();
std::shared_ptr<const Range> GuestAllocationsLookup_nid_postfix(std::uintptr_t address, bool next);
// Changes whenever a mutation ends, so callers can cache facts about guest mappings between changes.
std::uint64_t GuestAllocationsGeneration_nid_postfix();
// Precise invalidation for such caches: the callback runs, after the generation changed, for every
// range whose mapping or protection changed (registry mutations and guest heap decommits).
void GuestAllocationsSetInvalidator_nid_postfix(void (*callback)(std::uintptr_t address, std::size_t bytes));
void GuestAllocationsInvalidate_nid_postfix(std::uintptr_t address, std::size_t bytes);
// A mutation of a range that GPU work still leases waits for the lease (RequireUnpinned). The
// waiter, set by the GPU driver, is called on the mutating thread with the registry lock released
// and finishes the GPU work that holds leases; it returns whether it finished any (a round that found
// nothing to wait for counts against the same bound as a plain spin). Without one the mutation spins
// until the lease is dropped by another thread.
void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)(std::uintptr_t address, std::size_t bytes));
void GuestAllocationsSetGpuMapObserver_nid_postfix(void (*callback)(const Mapped& ranges, std::uint64_t generation));
void GuestAllocationsSetUnmapObserver_nid_postfix(void (*callback)(std::uintptr_t address, std::size_t bytes));
void GuestAllocationsNoteGpuMapping_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
}

class Mutation {
public:
    Mutation() : handle(GuestAllocationsBegin_nid_postfix()) {}
    ~Mutation() { GuestAllocationsEnd_nid_postfix(handle); }
    Mutation(const Mutation&) = delete;
    Mutation& operator=(const Mutation&) = delete;
    void RegisterMainImage() { GuestAllocationsRegisterMainImage_nid_postfix(handle); }
    void RegisterImage(const void* image) { GuestAllocationsRegisterImage_nid_postfix(handle, image); }
    void UnregisterImage(const void* image) { GuestAllocationsUnregisterImage_nid_postfix(handle, image); }
    void Add(void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu) { GuestAllocationsAdd_nid_postfix(handle, pointer, bytes, readable, writable, gpu); }
    void RequireUnpinned(const void* pointer, std::size_t bytes) const { GuestAllocationsRequireUnpinned_nid_postfix(handle, pointer, bytes); }
    void RequireAvailable(const void* pointer, std::size_t bytes) const { GuestAllocationsRequireAvailable_nid_postfix(handle, pointer, bytes); }
    bool Covers(const void* pointer, std::size_t bytes) const { return GuestAllocationsCovers_nid_postfix(handle, pointer, bytes); }
    bool Overlaps(const void* pointer, std::size_t bytes) const { return GuestAllocationsOverlaps_nid_postfix(handle, pointer, bytes); }
    bool ForRegistered(const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t)>& visit) const { return GuestAllocationsForRegistered_nid_postfix(handle, pointer, bytes, visit); }
    Range Find(const void* pointer) const { return GuestAllocationsFind_nid_postfix(handle, pointer); }
    void Remove(const void* pointer) { GuestAllocationsRemove_nid_postfix(handle, pointer); }
    void Unmap(const void* pointer, std::size_t bytes, const std::function<void(const void*, std::size_t, const void*, bool)>& apply) { GuestAllocationsUnmap_nid_postfix(handle, pointer, bytes, apply); }
    void Protect(const void* pointer, std::size_t bytes, bool readable, bool writable, bool gpu, const std::function<void()>& apply) { GuestAllocationsProtect_nid_postfix(handle, pointer, bytes, readable, writable, gpu, apply); }
    void NoteGpuMapping(const void* pointer, std::size_t bytes) { GuestAllocationsNoteGpuMapping_nid_postfix(handle, pointer, bytes); }

private:
    void* handle;
};

}

#endif
