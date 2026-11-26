#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using GuestCallback = void (APS5_VABI*)(void*);
using GuestDeleterQuery = const void* (APS5_VABI*)(const void*, const std::type_info*);

struct ControlBlockVtable {
    GuestCallback destroy;
    GuestCallback deleteObject;
    GuestCallback onZeroShared;
    GuestDeleterQuery getDeleter;
    GuestCallback onZeroSharedWeak;
};

struct ControlBlock {
    const ControlBlockVtable* vtable;
    std::int64_t sharedOwners;
    std::int64_t weakOwners;
};

static_assert(offsetof(ControlBlock, sharedOwners) == 8);
static_assert(offsetof(ControlBlock, weakOwners) == 16);
static_assert(offsetof(ControlBlockVtable, onZeroSharedWeak) == 32);

std::atomic_ref<std::int64_t> Owners(void* self, std::size_t offset) {
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    if (!self || address > UINTPTR_MAX - offset ||
        (address + offset) % std::atomic_ref<std::int64_t>::required_alignment != 0)
        throw std::invalid_argument("invalid libc++ control block address");
    auto* owners = reinterpret_cast<std::int64_t*>(address + offset);
    if (!GuestWritable_nid_no_patch(owners, sizeof(*owners)))
        throw std::invalid_argument("libc++ control block count is not writable");
    return std::atomic_ref<std::int64_t>(*owners);
}

void Finalize(void* self) {
    std::uintptr_t vtable = 0;
    constexpr auto callbackOffset = offsetof(ControlBlockVtable, onZeroSharedWeak);
    GuestCallback callback = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtable, self, sizeof(vtable)) || vtable == 0 ||
        vtable > UINTPTR_MAX - callbackOffset ||
        !GuestCopyIn_nid_no_patch(&callback, reinterpret_cast<const void*>(vtable + callbackOffset), sizeof(callback)) || !callback)
        throw std::invalid_argument("invalid libc++ control block finalizer");
    callback(self);
}

}

extern "C" {

void* APS5_VABI _ZNSt3__119__shared_weak_count4lockEv_nid_postfix(void* self) {
    const auto owners = Owners(self, offsetof(ControlBlock, sharedOwners));
    auto count = owners.load();
    while (count >= 0) {
        if (count == std::numeric_limits<std::int64_t>::max())
            throw std::invalid_argument("invalid libc++ shared owner count");
        if (owners.compare_exchange_weak(count, count + 1)) return self;
    }
    return nullptr;
}

void APS5_VABI _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(void* self) {
    const auto owners = Owners(self, offsetof(ControlBlock, weakOwners));
    auto count = owners.load(std::memory_order_acquire);
    while (count >= 0) {
        if (owners.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
            if (count == 0) Finalize(self);
            return;
        }
    }
    throw std::invalid_argument("invalid libc++ weak owner count");
}

const void* APS5_VABI _ZNKSt3__119__shared_weak_count13__get_deleterERKSt9type_info_nid_postfix(const void*, const std::type_info*) {
    return nullptr;
}

void APS5_VABI _ZNSt3__119__shared_weak_countD2Ev_nid_postfix(void*) {}
void APS5_VABI _ZNSt3__114__shared_countD2Ev_nid_postfix(void*) {}

}
