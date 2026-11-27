//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/memory.cpp

#include "prx/libc/include/LibcxxWeakCount.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

struct LibcxxWeakCountVtable {
    std::uintptr_t precedingSlots[4];
    void (APS5_VABI* onZeroSharedWeak)(LibcxxWeakCount*);
};

static_assert(offsetof(LibcxxWeakCount, sharedOwners) == 8);
static_assert(offsetof(LibcxxWeakCount, weakOwners) == 16);
static_assert(offsetof(LibcxxWeakCountVtable, onZeroSharedWeak) == 32);

namespace {

void CheckControlBlock(LibcxxWeakCount* self) {
    if (reinterpret_cast<std::uintptr_t>(self) % alignof(LibcxxWeakCount) != 0 || !GuestWritable_nid_no_patch(self, sizeof(*self)))
        throw std::invalid_argument("libc++ weak reference: invalid control block");
}

}

extern "C" {

LibcxxWeakCount* APS5_VABI _ZNSt3__119__shared_weak_count4lockEv_nid_postfix(LibcxxWeakCount* self) {
    CheckControlBlock(self);
    std::atomic_ref owners(self->sharedOwners);
    auto count = owners.load();
    while (count != UINT64_MAX) {
        if (owners.compare_exchange_weak(count, count + 1)) return self;
    }
    return nullptr;
}

void APS5_VABI _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(LibcxxWeakCount* self) {
    CheckControlBlock(self);
    std::atomic_ref owners(self->weakOwners);
    if (owners.fetch_sub(1, std::memory_order_acq_rel) != 0) return;
    LibcxxWeakCountVtable vtable{};
    if (!GuestCopyIn_nid_no_patch(&vtable, self->vtable, sizeof(vtable)) || !vtable.onZeroSharedWeak)
        throw std::invalid_argument("libc++ weak reference: invalid cleanup callback");
    vtable.onZeroSharedWeak(self);
}

}
