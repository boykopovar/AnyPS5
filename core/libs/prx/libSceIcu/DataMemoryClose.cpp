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
#include <cmemory.h>
#include <cstring>
#include <udatamem.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

extern "C" {

void APS5_VABI udata_close_nid_postfix(UDataMemory* data) {
    if (!data) return;

#if defined(_WIN32)
    if (data->map) {
        UnmapViewOfFile(data->pHeader);
        CloseHandle(data->map);
        data->pHeader = nullptr;
        data->map = nullptr;
    }
#else
    if (data->map) {
        const auto length = static_cast<char*>(data->map) - static_cast<char*>(data->mapAddr);
        munmap(data->mapAddr, static_cast<std::size_t>(length));
        data->pHeader = nullptr;
        data->map = nullptr;
        data->mapAddr = nullptr;
    }
#endif

    if (data->heapAllocated) {
        uprv_free(data);
    } else {
        std::memset(data, 0, sizeof(*data));
        data->length = -1;
    }
}

}
