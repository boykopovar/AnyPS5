/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfMetaAllocatorExports.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_map>

using NKWebKit::WtfAbi::MetaAllocator;
using NKWebKit::WtfAbi::MetaAllocatorFreeSpaceNode;
using NKWebKit::WtfAbi::MetaAllocatorHandle;
using NKWebKit::WtfAbi::MetaAllocatorHandleRef;
using NKWebKit::WtfAbi::MetaAllocatorStatistics;
using NKWebKit::WtfAbi::MetaAllocatorVtable;

namespace {

using AddressMap = std::unordered_map<std::uintptr_t, MetaAllocatorFreeSpaceNode*>;
using OccupancyMap = std::unordered_map<std::uintptr_t, std::size_t>;

constexpr std::uint8_t LockIsHeldBit = 1;

template<typename T>
T& Checked(T* ptr) {
    if (!ptr) throw std::invalid_argument("WTF::MetaAllocator requires an object");
    return *ptr;
}

AddressMap& StartMap(MetaAllocator& self) {
    return *static_cast<AddressMap*>(self.freeSpaceStartAddressMap);
}

AddressMap& EndMap(MetaAllocator& self) {
    return *static_cast<AddressMap*>(self.freeSpaceEndAddressMap);
}

OccupancyMap& Occupancy(MetaAllocator& self) {
    return *static_cast<OccupancyMap*>(self.pageOccupancyMap);
}

class GuestLocker {
public:
    explicit GuestLocker(std::uint8_t* lock) : m_lock(lock) {
        std::uint8_t expected = 0;
        if (std::atomic_ref<std::uint8_t>(*m_lock).compare_exchange_weak(expected, LockIsHeldBit)) return;
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN3WTF4Lock8lockSlowEv)(m_lock);
    }

    ~GuestLocker() {
        std::uint8_t expected = LockIsHeldBit;
        if (std::atomic_ref<std::uint8_t>(*m_lock).compare_exchange_weak(expected, 0)) return;
        reinterpret_cast<void (APS5_VABI*)(void*)>(&_ZN3WTF4Lock10unlockSlowEv)(m_lock);
    }

    GuestLocker(const GuestLocker&) = delete;
    GuestLocker& operator=(const GuestLocker&) = delete;

    const void* holder() const {
        return &m_lock;
    }

private:
    std::uint8_t* m_lock;
};

void APS5_VABI MetaAllocatorVtableD1(MetaAllocator* self) {
    _ZN3WTF13MetaAllocatorD2Ev(self);
}

std::uintptr_t APS5_VABI MetaAllocatorPureVirtual(MetaAllocator*) {
    throw std::logic_error("WTF::MetaAllocator pure virtual function called");
}

void APS5_VABI MetaAllocatorPureNotify(MetaAllocator*, void*, std::size_t) {
    throw std::logic_error("WTF::MetaAllocator pure virtual function called");
}

std::uintptr_t APS5_VABI MetaAllocatorPureAllocateNewSpace(MetaAllocator* self, std::size_t&) {
    return MetaAllocatorPureVirtual(self);
}

const MetaAllocatorVtable MetaAllocatorBaseVtable{
    MetaAllocatorVtableD1,
    _ZN3WTF13MetaAllocatorD0Ev,
    MetaAllocatorPureAllocateNewSpace,
    MetaAllocatorPureNotify,
    MetaAllocatorPureNotify,
    _ZN3WTF13MetaAllocator7releaseERKNS_6LockerINS_4LockEEERNS_19MetaAllocatorHandleE,
};

std::size_t RoundUp(const MetaAllocator& self, std::size_t sizeInBytes) {
    if (std::numeric_limits<std::size_t>::max() - self.allocationGranule <= sizeInBytes)
        throw std::length_error("WTF::MetaAllocator allocation size overflows");
    return (sizeInBytes + self.allocationGranule - 1) & ~(self.allocationGranule - 1);
}

MetaAllocatorFreeSpaceNode* AllocFreeSpaceNode() {
    return new (_ZN3WTF10fastMallocEm(sizeof(MetaAllocatorFreeSpaceNode))) MetaAllocatorFreeSpaceNode{};
}

void AddFreeSpace(MetaAllocator& self, std::uintptr_t start, std::size_t sizeInBytes) {
    std::uintptr_t end = start + sizeInBytes;
    AddressMap& startMap = StartMap(self);
    AddressMap& endMap = EndMap(self);

    auto leftNeighbor = endMap.find(start);
    auto rightNeighbor = startMap.find(end);

    if (leftNeighbor != endMap.end()) {
        MetaAllocatorFreeSpaceNode* leftNode = leftNeighbor->second;
        std::uintptr_t leftEnd = leftNode->end;

        self.freeSpaceSizeMap.remove(leftNode);
        endMap.erase(leftEnd);

        if (rightNeighbor != startMap.end()) {
            MetaAllocatorFreeSpaceNode* rightNode = rightNeighbor->second;
            std::uintptr_t rightStart = rightNeighbor->first;
            std::size_t rightSize = rightNode->sizeInBytes();
            std::uintptr_t rightEnd = rightNode->end;

            self.freeSpaceSizeMap.remove(rightNode);
            startMap.erase(rightStart);
            endMap.erase(rightEnd);

            _ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE(&self, rightNode);

            leftNode->end += sizeInBytes + rightSize;

            self.freeSpaceSizeMap.insert(leftNode);
            endMap.emplace(rightEnd, leftNode);
        } else {
            leftNode->end += sizeInBytes;

            self.freeSpaceSizeMap.insert(leftNode);
            endMap.emplace(end, leftNode);
        }
    } else if (rightNeighbor != startMap.end()) {
        MetaAllocatorFreeSpaceNode* rightNode = rightNeighbor->second;
        std::uintptr_t rightStart = rightNeighbor->first;

        self.freeSpaceSizeMap.remove(rightNode);
        startMap.erase(rightStart);

        rightNode->start = start;

        self.freeSpaceSizeMap.insert(rightNode);
        startMap.emplace(start, rightNode);
    } else {
        MetaAllocatorFreeSpaceNode* node = AllocFreeSpaceNode();

        node->start = start;
        node->end = start + sizeInBytes;

        self.freeSpaceSizeMap.insert(node);
        startMap.emplace(start, node);
        endMap.emplace(end, node);
    }
}

void AddFreeSpaceFromReleasedHandle(MetaAllocator& self, std::uintptr_t start, std::size_t sizeInBytes) {
    self.bytesAllocated -= sizeInBytes;
    AddFreeSpace(self, start, sizeInBytes);
}

std::uintptr_t FindAndRemoveFreeSpace(MetaAllocator& self, std::size_t sizeInBytes) {
    MetaAllocatorFreeSpaceNode* node = self.freeSpaceSizeMap.findLeastGreaterThanOrEqual(sizeInBytes);
    if (!node) return 0;

    std::size_t nodeSizeInBytes = node->sizeInBytes();
    self.freeSpaceSizeMap.remove(node);

    std::uintptr_t result;
    if (nodeSizeInBytes == sizeInBytes) {
        result = node->start;

        StartMap(self).erase(node->start);
        EndMap(self).erase(node->end);
        _ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE(&self, node);
    } else {
        std::uintptr_t nodeStart = node->start;
        std::uintptr_t firstPage = nodeStart >> self.logPageSize;
        std::uintptr_t lastPage = (nodeStart + nodeSizeInBytes - 1) >> self.logPageSize;

        std::uintptr_t lastPageForLeftAllocation = (nodeStart + sizeInBytes - 1) >> self.logPageSize;
        std::uintptr_t firstPageForRightAllocation = (nodeStart + nodeSizeInBytes - sizeInBytes) >> self.logPageSize;

        if (lastPageForLeftAllocation - firstPage + 1 <= lastPage - firstPageForRightAllocation + 1) {
            result = node->start;

            StartMap(self).erase(node->start);
            node->start += sizeInBytes;

            self.freeSpaceSizeMap.insert(node);
            StartMap(self).emplace(node->start, node);
        } else {
            result = node->end - sizeInBytes;

            EndMap(self).erase(node->end);
            node->end = result;

            self.freeSpaceSizeMap.insert(node);
            EndMap(self).emplace(result, node);
        }
    }
    return result;
}

void IncrementPageOccupancy(MetaAllocator& self, std::uintptr_t address, std::size_t sizeInBytes) {
    std::uintptr_t firstPage = address >> self.logPageSize;
    std::uintptr_t lastPage = (address + sizeInBytes - 1) >> self.logPageSize;

    std::uintptr_t currentPageStart = 0;
    std::size_t count = 0;
    auto flushNeedPages = [&] {
        if (!currentPageStart) return;
        self.vptr->notifyNeedPage(&self, reinterpret_cast<void*>(currentPageStart << self.logPageSize), count);
        currentPageStart = 0;
        count = 0;
    };

    for (std::uintptr_t page = firstPage; page <= lastPage; ++page) {
        auto [iterator, isNewEntry] = Occupancy(self).emplace(page, 1);
        if (isNewEntry) {
            self.bytesCommitted += self.pageSize;
            if (!currentPageStart) currentPageStart = page;
            ++count;
        } else {
            ++iterator->second;
            flushNeedPages();
        }
    }
    flushNeedPages();
}

void DecrementPageOccupancy(MetaAllocator& self, std::uintptr_t address, std::size_t sizeInBytes) {
    std::uintptr_t firstPage = address >> self.logPageSize;
    std::uintptr_t lastPage = (address + sizeInBytes - 1) >> self.logPageSize;

    std::uintptr_t currentPageStart = 0;
    std::size_t count = 0;
    auto flushFreePages = [&] {
        if (!currentPageStart) return;
        self.vptr->notifyPageIsFree(&self, reinterpret_cast<void*>(currentPageStart << self.logPageSize), count);
        currentPageStart = 0;
        count = 0;
    };

    for (std::uintptr_t page = firstPage; page <= lastPage; ++page) {
        auto iterator = Occupancy(self).find(page);
        if (iterator == Occupancy(self).end()) throw std::logic_error("WTF::MetaAllocator released a page it does not occupy");
        if (!--iterator->second) {
            Occupancy(self).erase(iterator);
            self.bytesCommitted -= self.pageSize;
            if (!currentPageStart) currentPageStart = page;
            ++count;
        } else
            flushFreePages();
    }
    flushFreePages();
}

void DestroyHandle(MetaAllocatorHandle* self) {
    MetaAllocator& allocator = Checked(Checked(self).allocator);
    GuestLocker locker(allocator.lock);
    allocator.vptr->release(&allocator, locker.holder(), *self);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13MetaAllocatorC2ERNS_4LockEmm(MetaAllocator* self, std::uint8_t* lock, std::size_t allocationGranule, std::size_t pageSize) {
    MetaAllocator& allocator = Checked(self);
    allocator.vptr = &MetaAllocatorBaseVtable;
    allocator.allocationGranule = allocationGranule;
    allocator.pageSize = pageSize;
    allocator.freeSpaceSizeMap.m_root = nullptr;
    allocator.freeSpaceStartAddressMap = new AddressMap;
    allocator.freeSpaceEndAddressMap = new AddressMap;
    allocator.pageOccupancyMap = new OccupancyMap;
    allocator.bytesAllocated = 0;
    allocator.bytesReserved = 0;
    allocator.bytesCommitted = 0;
    allocator.lock = &Checked(lock);
    allocator.tracker = nullptr;

    for (allocator.logPageSize = 0; allocator.logPageSize < 32; ++allocator.logPageSize) {
        if (static_cast<std::size_t>(1) << allocator.logPageSize == allocator.pageSize) break;
    }
    for (allocator.logAllocationGranule = 0; allocator.logAllocationGranule < 32; ++allocator.logAllocationGranule) {
        if (static_cast<std::size_t>(1) << allocator.logAllocationGranule == allocator.allocationGranule) break;
    }
}

void APS5_VABI _ZN3WTF13MetaAllocatorD2Ev(MetaAllocator* self) {
    MetaAllocator& allocator = Checked(self);
    allocator.vptr = &MetaAllocatorBaseVtable;
    for (MetaAllocatorFreeSpaceNode* node = allocator.freeSpaceSizeMap.first(); node;) {
        MetaAllocatorFreeSpaceNode* next = node->successor();
        allocator.freeSpaceSizeMap.remove(node);
        _ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE(&allocator, node);
        node = next;
    }
    delete static_cast<AddressMap*>(allocator.freeSpaceStartAddressMap);
    delete static_cast<AddressMap*>(allocator.freeSpaceEndAddressMap);
    delete static_cast<OccupancyMap*>(allocator.pageOccupancyMap);
    allocator.freeSpaceStartAddressMap = nullptr;
    allocator.freeSpaceEndAddressMap = nullptr;
    allocator.pageOccupancyMap = nullptr;
}

void APS5_VABI _ZN3WTF13MetaAllocatorD0Ev(MetaAllocator* self) {
    _ZN3WTF13MetaAllocatorD2Ev(self);
    _ZdlPvm_nid_postfix(self, sizeof(MetaAllocator));
}

void APS5_VABI _ZN3WTF13MetaAllocator7releaseERKNS_6LockerINS_4LockEEERNS_19MetaAllocatorHandleE(MetaAllocator* self, const void*, MetaAllocatorHandle& handle) {
    MetaAllocator& allocator = Checked(self);
    if (std::size_t sizeInBytes = handle.sizeInBytes()) {
        std::uintptr_t start = handle.start;
        DecrementPageOccupancy(allocator, start, sizeInBytes);
        AddFreeSpaceFromReleasedHandle(allocator, start, sizeInBytes);
    }
    if (allocator.tracker) allocator.tracker->allocations.remove(&handle);
}

MetaAllocatorHandleRef APS5_VABI _ZN3WTF13MetaAllocator8allocateERKNS_6LockerINS_4LockEEEm(MetaAllocator* self, const void*, std::size_t sizeInBytes) {
    MetaAllocator& allocator = Checked(self);
    if (!sizeInBytes) return MetaAllocatorHandleRef{};

    sizeInBytes = RoundUp(allocator, sizeInBytes);

    std::uintptr_t start = FindAndRemoveFreeSpace(allocator, sizeInBytes);
    if (!start) {
        std::size_t numberOfPages = (sizeInBytes + allocator.pageSize - 1) >> allocator.logPageSize;

        start = allocator.vptr->allocateNewSpace(&allocator, numberOfPages);
        if (!start) return MetaAllocatorHandleRef{};

        std::size_t roundedUpSize = numberOfPages << allocator.logPageSize;
        allocator.bytesReserved += roundedUpSize;

        if (roundedUpSize > sizeInBytes) AddFreeSpace(allocator, start + sizeInBytes, roundedUpSize - sizeInBytes);
    }
    IncrementPageOccupancy(allocator, start, sizeInBytes);
    allocator.bytesAllocated += sizeInBytes;

    auto* handle = new (_ZN3WTF10fastMallocEm(sizeof(MetaAllocatorHandle))) MetaAllocatorHandle{};
    handle->refCount = 1;
    handle->allocator = &allocator;
    handle->start = start;
    handle->end = start + sizeInBytes;

    if (allocator.tracker) allocator.tracker->allocations.insert(handle);
    return MetaAllocatorHandleRef{handle};
}

MetaAllocatorStatistics APS5_VABI _ZN3WTF13MetaAllocator17currentStatisticsERKNS_6LockerINS_4LockEEE(MetaAllocator* self, const void*) {
    MetaAllocator& allocator = Checked(self);
    return MetaAllocatorStatistics{allocator.bytesAllocated, allocator.bytesReserved, allocator.bytesCommitted};
}

void APS5_VABI _ZN3WTF13MetaAllocator17addFreshFreeSpaceEPvm(MetaAllocator* self, void* start, std::size_t sizeInBytes) {
    MetaAllocator& allocator = Checked(self);
    GuestLocker locker(allocator.lock);
    allocator.bytesReserved += sizeInBytes;
    AddFreeSpace(allocator, reinterpret_cast<std::uintptr_t>(start), sizeInBytes);
}

std::size_t APS5_VABI _ZN3WTF13MetaAllocator18debugFreeSpaceSizeEv(MetaAllocator* self) {
    MetaAllocator& allocator = Checked(self);
    GuestLocker locker(allocator.lock);
    std::size_t result = 0;
    for (MetaAllocatorFreeSpaceNode* node = allocator.freeSpaceSizeMap.first(); node; node = node->successor()) result += node->sizeInBytes();
    return result;
}

bool APS5_VABI _ZN3WTF13MetaAllocator19isInAllocatedMemoryERKNS_14AbstractLockerEPv(MetaAllocator* self, const void*, void* address) {
    MetaAllocator& allocator = Checked(self);
    return Occupancy(allocator).contains(reinterpret_cast<std::uintptr_t>(address) >> allocator.logPageSize);
}

void APS5_VABI _ZN3WTF13MetaAllocator17freeFreeSpaceNodeEPNS0_13FreeSpaceNodeE(MetaAllocator*, MetaAllocatorFreeSpaceNode* node) {
    _ZN3WTF8fastFreeEPv(node);
}

void APS5_VABI _ZN3WTF19MetaAllocatorHandleD2Ev(MetaAllocatorHandle* self) {
    DestroyHandle(self);
}

void APS5_VABI _ZN3WTF19MetaAllocatorHandleD1Ev(MetaAllocatorHandle* self) {
    DestroyHandle(self);
}

void APS5_VABI _ZN3WTF19MetaAllocatorHandle6shrinkEm(MetaAllocatorHandle* self, std::size_t newSizeInBytes) {
    MetaAllocatorHandle& handle = Checked(self);
    std::size_t sizeInBytes = handle.sizeInBytes();
    if (newSizeInBytes > sizeInBytes) throw std::invalid_argument("WTF::MetaAllocatorHandle::shrink cannot grow an allocation");

    MetaAllocator& allocator = Checked(handle.allocator);
    GuestLocker locker(allocator.lock);

    newSizeInBytes = RoundUp(allocator, newSizeInBytes);
    if (newSizeInBytes == sizeInBytes) return;

    std::uintptr_t freeStart = handle.start + newSizeInBytes;
    std::size_t freeSize = sizeInBytes - newSizeInBytes;
    std::uintptr_t freeEnd = freeStart + freeSize;

    std::uintptr_t firstCompletelyFreePage = (freeStart + allocator.pageSize - 1) & ~(allocator.pageSize - 1);
    if (firstCompletelyFreePage < freeEnd) DecrementPageOccupancy(allocator, firstCompletelyFreePage, freeSize - (firstCompletelyFreePage - freeStart));

    AddFreeSpaceFromReleasedHandle(allocator, freeStart, freeSize);

    handle.end = freeStart;
}

}
