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
};

using Lease = std::vector<std::shared_ptr<const Range>>;

extern "C" {
void* GuestAllocationsBegin_nid_postfix();
#ifdef _WIN32
void GuestAllocationsRegisterMainImage_nid_postfix(void* mutation);
#endif
void GuestAllocationsEnd_nid_postfix(void* mutation) noexcept;
void GuestAllocationsAdd_nid_postfix(void* mutation, void* pointer, std::size_t bytes, bool readable, bool writable);
void GuestAllocationsRequireUnpinned_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
void GuestAllocationsRequireAvailable_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
bool GuestAllocationsCovers_nid_postfix(void* mutation, const void* pointer, std::size_t bytes);
Range GuestAllocationsFind_nid_postfix(void* mutation, const void* pointer);
void GuestAllocationsRemove_nid_postfix(void* mutation, const void* pointer);
void GuestAllocationsProtect_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, bool readable, bool writable, const std::function<void()>& apply);
void GuestAllocationsUnmap_nid_postfix(void* mutation, const void* pointer, std::size_t bytes, const std::function<void(const void*, bool)>& apply);
Lease GuestAllocationsAcquire_nid_postfix();
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
void GuestAllocationsSetPinWaiter_nid_postfix(bool (*callback)());
}

class Mutation {
public:
    Mutation() : handle(GuestAllocationsBegin_nid_postfix()) {}
    ~Mutation() { GuestAllocationsEnd_nid_postfix(handle); }
    Mutation(const Mutation&) = delete;
    Mutation& operator=(const Mutation&) = delete;
#ifdef _WIN32
    void RegisterMainImage() { GuestAllocationsRegisterMainImage_nid_postfix(handle); }
#endif
    void Add(void* pointer, std::size_t bytes, bool readable, bool writable) { GuestAllocationsAdd_nid_postfix(handle, pointer, bytes, readable, writable); }
    void RequireUnpinned(const void* pointer, std::size_t bytes) const { GuestAllocationsRequireUnpinned_nid_postfix(handle, pointer, bytes); }
    void RequireAvailable(const void* pointer, std::size_t bytes) const { GuestAllocationsRequireAvailable_nid_postfix(handle, pointer, bytes); }
    bool Covers(const void* pointer, std::size_t bytes) const { return GuestAllocationsCovers_nid_postfix(handle, pointer, bytes); }
    Range Find(const void* pointer) const { return GuestAllocationsFind_nid_postfix(handle, pointer); }
    void Remove(const void* pointer) { GuestAllocationsRemove_nid_postfix(handle, pointer); }
    void Unmap(const void* pointer, std::size_t bytes, const std::function<void(const void*, bool)>& apply) { GuestAllocationsUnmap_nid_postfix(handle, pointer, bytes, apply); }
    void Protect(const void* pointer, std::size_t bytes, bool readable, bool writable, const std::function<void()>& apply) { GuestAllocationsProtect_nid_postfix(handle, pointer, bytes, readable, writable, apply); }

private:
    void* handle;
};

}

#endif
