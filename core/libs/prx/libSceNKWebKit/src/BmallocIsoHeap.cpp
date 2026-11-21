// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoHeapImpl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoHeapImpl.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoHeapImplInlines.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoTLSEntry.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoTLSEntry.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/IsoTLSLayout.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/bmalloc/bmalloc/AllIsoHeaps.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/BmallocIsoHeapExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <atomic>
#include <climits>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::BmallocAbi::AllIsoHeaps;
using NKWebKit::BmallocAbi::IsoHeapImplBase;
using NKWebKit::BmallocAbi::IsoTLSEntry;
using NKWebKit::BmallocAbi::IsoTLSLayout;

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

void ReleaseAssert(bool condition, const char* function) {
    if (!condition) throw std::logic_error(std::string(function) + ": RELEASE_BASSERT failed");
}

void APS5_VABI HeapCompleteDestructor(void*) {
}

void APS5_VABI HeapDeletingDestructor(void* self) {
    _ZN7bmalloc15IsoHeapImplBaseD0Ev(static_cast<IsoHeapImplBase*>(self));
}

void APS5_VABI HeapPureScavenge(void*, void*) {
    throw std::logic_error("bmalloc::IsoHeapImplBase::scavenge is pure virtual");
}

void APS5_VABI EntryCompleteDestructor(void*) {
}

void APS5_VABI EntryDeletingDestructor(void* self) {
    _ZN7bmalloc11IsoTLSEntryD0Ev(static_cast<IsoTLSEntry*>(self));
}

void APS5_VABI EntryPureVirtual(void*) {
    throw std::logic_error("bmalloc::IsoTLSEntry pure virtual function called");
}

const void* const HeapVtable[] = {reinterpret_cast<const void*>(&HeapCompleteDestructor), reinterpret_cast<const void*>(&HeapDeletingDestructor),
    reinterpret_cast<const void*>(&HeapPureScavenge)};
const void* const EntryVtable[] = {reinterpret_cast<const void*>(&EntryCompleteDestructor), reinterpret_cast<const void*>(&EntryDeletingDestructor),
    reinterpret_cast<const void*>(&EntryPureVirtual), reinterpret_cast<const void*>(&EntryPureVirtual), reinterpret_cast<const void*>(&EntryPureVirtual),
    reinterpret_cast<const void*>(&EntryPureVirtual)};

std::mutex& AllIsoHeapsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::size_t Extent(const IsoTLSEntry& entry) {
    return static_cast<std::uint32_t>(entry.offset + entry.size);
}

constexpr std::size_t EntryAlignment = sizeof(std::uint64_t);

}  // namespace

extern "C" {

void APS5_VABI _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(IsoHeapImplBase* self, void* lock) {
    if (!lock) throw std::invalid_argument("bmalloc::IsoHeapImplBase::IsoHeapImplBase requires a lock");
    auto& heap = Checked(self, "bmalloc::IsoHeapImplBase::IsoHeapImplBase");
    heap.vptr = HeapVtable;
    heap.lock = lock;
    heap.next = nullptr;
    heap.lastSlowPathTime = 0;
    heap.footprint = 0;
    heap.freeableMemory = 0;
    std::memset(heap.sharedCells, 0, sizeof(heap.sharedCells));
    heap.numberOfAllocationsFromSharedInOneCycle = 0;
    heap.availableShared = 0xFF;
    heap.allocationMode = 0;
    heap.isInlineDirectoryEligibleOrDecommitted = true;
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBaseD0Ev(IsoHeapImplBase* self) {
    Checked(self, "bmalloc::IsoHeapImplBase::~IsoHeapImplBase").vptr = HeapVtable;
    _ZN7bmalloc15IsoHeapImplBasedlEPv(self);
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBase9didCommitEPvm(IsoHeapImplBase* self, void*, std::size_t bytes) {
    Checked(self, "bmalloc::IsoHeapImplBase::didCommit").footprint += bytes;
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBase11didDecommitEPvm(IsoHeapImplBase* self, void*, std::size_t bytes) {
    Checked(self, "bmalloc::IsoHeapImplBase::didDecommit").footprint -= bytes;
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBase13isNowFreeableEPvm(IsoHeapImplBase* self, void*, std::size_t bytes) {
    Checked(self, "bmalloc::IsoHeapImplBase::isNowFreeable").freeableMemory += bytes;
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBase18isNoLongerFreeableEPvm(IsoHeapImplBase* self, void*, std::size_t bytes) {
    Checked(self, "bmalloc::IsoHeapImplBase::isNoLongerFreeable").freeableMemory -= bytes;
}

void APS5_VABI _ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv(IsoHeapImplBase* self) {
    _ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE(BmallocHostAllIsoHeaps_nid_no_patch(), &Checked(self, "bmalloc::IsoHeapImplBase::addToAllIsoHeaps"));
}

void APS5_VABI _ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE(AllIsoHeaps* self, IsoHeapImplBase* heap) {
    auto& heaps = Checked(self, "bmalloc::AllIsoHeaps::add");
    auto& added = Checked(heap, "bmalloc::AllIsoHeaps::add");
    std::lock_guard<std::mutex> locker(AllIsoHeapsMutex());
    added.next = heaps.head;
    heaps.head = &added;
}

IsoHeapImplBase* APS5_VABI _ZN7bmalloc11AllIsoHeaps4headEv(AllIsoHeaps* self) {
    auto& heaps = Checked(self, "bmalloc::AllIsoHeaps::head");
    std::lock_guard<std::mutex> locker(AllIsoHeapsMutex());
    return heaps.head;
}

AllIsoHeaps* BmallocHostAllIsoHeaps_nid_no_patch() {
    static AllIsoHeaps heaps{nullptr};
    return &heaps;
}

void APS5_VABI _ZN7bmalloc11IsoTLSEntryC2Em(IsoTLSEntry* self, std::size_t size) {
    auto& entry = Checked(self, "bmalloc::IsoTLSEntry::IsoTLSEntry");
    entry.vptr = EntryVtable;
    entry.next = nullptr;
    entry.offset = UINT_MAX;
    entry.size = static_cast<std::uint32_t>(size);
}

void APS5_VABI _ZN7bmalloc11IsoTLSEntryD0Ev(IsoTLSEntry* self) {
    Checked(self, "bmalloc::IsoTLSEntry::~IsoTLSEntry").vptr = EntryVtable;
    _ZN7bmalloc11IsoTLSEntrydlEPv(self);
}

std::size_t APS5_VABI _ZNK7bmalloc11IsoTLSEntry6extentEv(const IsoTLSEntry* self) {
    return Extent(Checked(self, "bmalloc::IsoTLSEntry::extent"));
}

void APS5_VABI _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(IsoTLSLayout* self, IsoTLSEntry* entry) {
    static std::mutex addingMutex;
    auto& layout = Checked(self, "bmalloc::IsoTLSLayout::add");
    auto& added = Checked(entry, "bmalloc::IsoTLSLayout::add");
    ReleaseAssert(!added.next, "bmalloc::IsoTLSLayout::add");
    std::lock_guard<std::mutex> locking(addingMutex);
    if (layout.head) {
        ReleaseAssert(layout.tail, "bmalloc::IsoTLSLayout::add");
        const std::size_t extent = Extent(*layout.tail);
        const std::size_t offset = (extent + EntryAlignment - 1) / EntryAlignment * EntryAlignment;
        ReleaseAssert(offset < UINT_MAX, "bmalloc::IsoTLSLayout::add");
        added.offset = static_cast<std::uint32_t>(offset);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        layout.tail->next = &added;
        layout.tail = &added;
    } else {
        ReleaseAssert(!layout.tail, "bmalloc::IsoTLSLayout::add");
        added.offset = 0;
        std::atomic_thread_fence(std::memory_order_seq_cst);
        layout.head = &added;
        layout.tail = &added;
    }
}

}
