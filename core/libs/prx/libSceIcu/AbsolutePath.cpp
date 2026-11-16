// Derived from icu4c/source/common/putil.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_pathIsAbsolute.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include <cstdint>

namespace {

constexpr char FileSeparator = '/';

}

extern "C" {

std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    if (path == nullptr || *path == 0) return 0;
    return *path == FileSeparator ? 1 : 0;
}

}
