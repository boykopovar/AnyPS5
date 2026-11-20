// Derived from icu4c/source/common/ucnv.c@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the CCSID
// converter opener below forwards to the host ICU entry point the rest of this library uses.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucnv.c
//
// The upstream file's notice, verbatim:
//
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1996-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/ConverterCcsid.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cerrno>
#include <cstdint>

extern "C" UConverter* APS5_VABI ucnv_openCCSID_nid_postfix(std::int32_t codepage, std::int32_t platform,
    std::int32_t* status) {
    if (status == nullptr) return nullptr;
    UErrorCode local = U_ZERO_ERROR;
    {
        const int saved = errno;
        GuestCopyIn_nid_no_patch(&local, status, sizeof(local));
        errno = saved;
    }
    if (local > 0) return nullptr;
    if (platform < 0 || platform > 2) {
        local = U_ILLEGAL_ARGUMENT_ERROR;
        const int saved = errno;
        GuestCopyOut_nid_no_patch(status, &local, sizeof(local));
        errno = saved;
        return nullptr;
    }
    UConverter* converter = ucnv_openCCSID(codepage, static_cast<UConverterPlatform>(platform), &local);
    {
        const int saved = errno;
        GuestCopyOut_nid_no_patch(status, &local, sizeof(local));
        errno = saved;
    }
    return converter;
}
