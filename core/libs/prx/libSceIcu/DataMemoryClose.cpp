// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udatamem.cpp

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/DataMemoryOwnership.hpp"
#include "prx/libc/include/General.hpp"
#include <cmemory.h>
#include <cerrno>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <udatamem.h>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace IcuDataMemoryOwnership {

namespace {

constexpr std::size_t MaxFreedEntries = 4096;
std::mutex registryMutex;
std::unordered_map<void*, TrustedRecord> registry;
std::unordered_set<void*> freed;
std::deque<void*> freedOrder;

}

void MarkFreed(void* pointer) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    if (freed.insert(pointer).second) {
        freedOrder.push_back(pointer);
        if (freedOrder.size() > MaxFreedEntries) {
            freed.erase(freedOrder.front());
            freedOrder.pop_front();
        }
    }
}

bool IsFreed(void* pointer) {
    if (!pointer) return false;
    std::lock_guard<std::mutex> lock(registryMutex);
    return freed.find(pointer) != freed.end();
}

void Register(void* pointer, const void* pHeader, void* mapAddr, void* map) {
    if (!pointer) return;
    std::lock_guard<std::mutex> lock(registryMutex);
    freed.erase(pointer);
    registry[pointer] = TrustedRecord{pHeader, mapAddr, map};
}

bool TryTake(void* pointer, TrustedRecord& out) {
    if (!pointer) return false;
    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = registry.find(pointer);
    if (it == registry.end()) return false;
    out = it->second;
    registry.erase(it);
    return true;
}

}

extern "C" {

void APS5_VABI udata_close_nid_postfix(UDataMemory* data) {
    if (!data) return;
    if (IcuDataMemoryOwnership::IsFreed(data)) return;

    const int savedErrno = errno;
    UDataMemory local{};
    const bool readable = GuestCopyIn_nid_no_patch(&local, data, sizeof(local));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument(__func__);

    IcuDataMemoryOwnership::TrustedRecord trusted{};
    if (IcuDataMemoryOwnership::TryTake(data, trusted)) {
#if defined(_WIN32)
        if (trusted.map && trusted.pHeader) {
            UnmapViewOfFile(const_cast<void*>(trusted.pHeader));
            CloseHandle(static_cast<HANDLE>(trusted.map));
        }
#else
        if (trusted.map && trusted.mapAddr && trusted.pHeader) {
            const auto length = static_cast<char*>(trusted.map) - static_cast<char*>(trusted.mapAddr);
            if (length > 0) munmap(trusted.mapAddr, static_cast<std::size_t>(length));
        }
#endif
        IcuDataMemoryOwnership::MarkFreed(data);
        uprv_free(data);
        errno = savedErrno;
        return;
    }

    UDataMemory cleared{};
    cleared.length = -1;
    const int savedCopyErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(data, &cleared, sizeof(cleared));
    errno = savedCopyErrno;
    if (!written) throw std::invalid_argument(__func__);
}

}
