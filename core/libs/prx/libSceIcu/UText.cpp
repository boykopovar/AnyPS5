// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/utext.cpp
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2005-2016, International Business Machines
// Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/UText.hpp"

namespace {

constexpr std::uint32_t TextMagic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;

} // namespace

extern "C" {

UText* APS5_VABI utext_close(UText* ut) {
    if (!ut || ut->magic != TextMagic || !(ut->flags & Open)) return ut;
    if (ut->pFuncs->close) ut->pFuncs->close(ut);
    ut->flags &= ~Open;
    if (ut->flags & ExtraHeapAllocated) {
        IcuReleaseMemory_nid_no_patch(ut->pExtra);
        ut->pExtra = nullptr;
        ut->flags &= ~ExtraHeapAllocated;
        ut->extraSize = 0;
    }
    ut->pFuncs = nullptr;
    if (!(ut->flags & HeapAllocated)) return ut;
    ut->magic = 0;
    IcuReleaseMemory_nid_no_patch(ut);
    return nullptr;
}

}
