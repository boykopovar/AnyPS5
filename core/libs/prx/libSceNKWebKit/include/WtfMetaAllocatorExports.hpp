#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfMetaAllocatorAbi.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>

extern "C" {

void APS5_VABI _ZN3WTF13MetaAllocatorC2ERNS_4LockEmm(NKWebKit::WtfAbi::MetaAllocator* self, std::uint8_t* lock, std::size_t allocationGranule, std::size_t pageSize);
void APS5_VABI _ZN3WTF13MetaAllocatorD2Ev(NKWebKit::WtfAbi::MetaAllocator* self);
void APS5_VABI _ZN3WTF13MetaAllocatorD0Ev(NKWebKit::WtfAbi::MetaAllocator* self);
void APS5_VABI _ZN3WTF13MetaAllocator7releaseERKNS_6LockerINS_4LockEEERNS_19MetaAllocatorHandleE(NKWebKit::WtfAbi::MetaAllocator* self, const void* locker, NKWebKit::WtfAbi::MetaAllocatorHandle& handle);
NKWebKit::WtfAbi::MetaAllocatorHandleRef APS5_VABI _ZN3WTF13MetaAllocator8allocateERKNS_6LockerINS_4LockEEEm(NKWebKit::WtfAbi::MetaAllocator* self, const void* locker, std::size_t sizeInBytes);
NKWebKit::WtfAbi::MetaAllocatorStatistics APS5_VABI _ZN3WTF13MetaAllocator17currentStatisticsERKNS_6LockerINS_4LockEEE(NKWebKit::WtfAbi::MetaAllocator* self, const void* locker);
void APS5_VABI _ZN3WTF13MetaAllocator17addFreshFreeSpaceEPvm(NKWebKit::WtfAbi::MetaAllocator* self, void* start, std::size_t sizeInBytes);
std::size_t APS5_VABI _ZN3WTF13MetaAllocator18debugFreeSpaceSizeEv(NKWebKit::WtfAbi::MetaAllocator* self);
bool APS5_VABI _ZN3WTF13MetaAllocator19isInAllocatedMemoryERKNS_14AbstractLockerEPv(NKWebKit::WtfAbi::MetaAllocator* self, const void* locker, void* address);
void APS5_VABI _ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE(NKWebKit::WtfAbi::MetaAllocator* self, NKWebKit::WtfAbi::MetaAllocatorFreeSpaceNode* node);
void APS5_VABI _ZN3WTF19MetaAllocatorHandleD1Ev(NKWebKit::WtfAbi::MetaAllocatorHandle* self);
void APS5_VABI _ZN3WTF19MetaAllocatorHandleD2Ev(NKWebKit::WtfAbi::MetaAllocatorHandle* self);
void APS5_VABI _ZN3WTF19MetaAllocatorHandle6shrinkEm(NKWebKit::WtfAbi::MetaAllocatorHandle* self, std::size_t newSizeInBytes);

}

inline NKWebKit::WtfAbi::MetaAllocatorHandleRef::~MetaAllocatorHandleRef() {
    if (!ptr) return;
    if (std::atomic_ref<std::uint32_t>(ptr->refCount).fetch_sub(1) != 1) return;
    _ZN3WTF19MetaAllocatorHandleD1Ev(ptr);
    _ZN3WTF8fastFreeEPv(ptr);
}
