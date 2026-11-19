// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstring>

namespace {

constexpr std::size_t Icu68ResourceBundleSize = 200;

}

extern "C" {

void APS5_VABI ures_close(UResourceBundle* resB) {
    if (!resB) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ures_initStackObject_nid_postfix(UResourceBundle* resB) {
    std::memset(resB, 0, Icu68ResourceBundleSize);
}

}
