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
#include <stdexcept>
#include <udatamem.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

extern "C" {

void APS5_VABI udata_close_nid_postfix(UDataMemory* data) {
    if (!data) return;

    const int savedErrno = errno;
    UDataMemory local{};
    const bool readable = GuestCopyIn_nid_no_patch(&local, data, sizeof(local));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument(__func__);

#if defined(_WIN32)
    if (local.map) {
        UnmapViewOfFile(local.pHeader);
        CloseHandle(local.map);
        local.pHeader = nullptr;
        local.map = nullptr;
    }
#else
    if (local.map) {
        const auto length = static_cast<char*>(local.map) - static_cast<char*>(local.mapAddr);
        munmap(local.mapAddr, static_cast<std::size_t>(length));
        local.pHeader = nullptr;
        local.map = nullptr;
        local.mapAddr = nullptr;
    }
#endif

    if (IcuDataMemoryOwnership::TryTake(data)) {
        uprv_free(data);
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
