#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::BmallocAbi {

struct IsoHeapImplBase {
    const void* const* vptr;
    void* lock;
    IsoHeapImplBase* next;
    std::int64_t lastSlowPathTime;
    std::uint64_t footprint;
    std::uint64_t freeableMemory;
    std::uint8_t sharedCells[48];
    std::uint32_t numberOfAllocationsFromSharedInOneCycle;
    std::uint32_t availableShared;
    std::uint8_t allocationMode;
    bool isInlineDirectoryEligibleOrDecommitted;
    std::uint8_t padding[6];
};

struct IsoTLSEntry {
    const void* const* vptr;
    IsoTLSEntry* next;
    std::uint32_t offset;
    std::uint32_t size;
};

struct IsoTLSLayout {
    IsoTLSEntry* head;
    IsoTLSEntry* tail;
};

struct AllIsoHeaps {
    IsoHeapImplBase* head;
};

static_assert(std::is_standard_layout_v<IsoHeapImplBase>);
static_assert(sizeof(IsoHeapImplBase) == 112);
static_assert(offsetof(IsoHeapImplBase, next) == 16);
static_assert(offsetof(IsoHeapImplBase, footprint) == 32);
static_assert(offsetof(IsoHeapImplBase, freeableMemory) == 40);
static_assert(offsetof(IsoHeapImplBase, sharedCells) == 48);
static_assert(offsetof(IsoHeapImplBase, numberOfAllocationsFromSharedInOneCycle) == 96);
static_assert(offsetof(IsoHeapImplBase, availableShared) == 100);
static_assert(offsetof(IsoHeapImplBase, allocationMode) == 104);
static_assert(offsetof(IsoHeapImplBase, isInlineDirectoryEligibleOrDecommitted) == 105);
static_assert(std::is_standard_layout_v<IsoTLSEntry>);
static_assert(sizeof(IsoTLSEntry) == 24);
static_assert(offsetof(IsoTLSEntry, offset) == 16);
static_assert(offsetof(IsoTLSEntry, size) == 20);
static_assert(sizeof(IsoTLSLayout) == 16);
static_assert(sizeof(AllIsoHeaps) == 8);

}  // namespace NKWebKit::BmallocAbi

extern "C" {

void APS5_VABI _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(NKWebKit::BmallocAbi::IsoHeapImplBase* self, void* lock);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBaseD0Ev(NKWebKit::BmallocAbi::IsoHeapImplBase* self);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBase9didCommitEPvm(NKWebKit::BmallocAbi::IsoHeapImplBase* self, void* ptr, std::size_t bytes);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBase11didDecommitEPvm(NKWebKit::BmallocAbi::IsoHeapImplBase* self, void* ptr, std::size_t bytes);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBase13isNowFreeableEPvm(NKWebKit::BmallocAbi::IsoHeapImplBase* self, void* ptr, std::size_t bytes);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBase18isNoLongerFreeableEPvm(NKWebKit::BmallocAbi::IsoHeapImplBase* self, void* ptr, std::size_t bytes);
void APS5_VABI _ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv(NKWebKit::BmallocAbi::IsoHeapImplBase* self);
void APS5_VABI _ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE(NKWebKit::BmallocAbi::AllIsoHeaps* self, NKWebKit::BmallocAbi::IsoHeapImplBase* heap);
NKWebKit::BmallocAbi::IsoHeapImplBase* APS5_VABI _ZN7bmalloc11AllIsoHeaps4headEv(NKWebKit::BmallocAbi::AllIsoHeaps* self);
void APS5_VABI _ZN7bmalloc11IsoTLSEntryC2Em(NKWebKit::BmallocAbi::IsoTLSEntry* self, std::size_t size);
void APS5_VABI _ZN7bmalloc11IsoTLSEntryD0Ev(NKWebKit::BmallocAbi::IsoTLSEntry* self);
std::size_t APS5_VABI _ZNK7bmalloc11IsoTLSEntry6extentEv(const NKWebKit::BmallocAbi::IsoTLSEntry* self);
void APS5_VABI _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(NKWebKit::BmallocAbi::IsoTLSLayout* self, NKWebKit::BmallocAbi::IsoTLSEntry* entry);
NKWebKit::BmallocAbi::AllIsoHeaps* BmallocHostAllIsoHeaps_nid_no_patch();

}
