// Derived from icu4c/source/common/uarrsort.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2013, International Business Machines
//   Corporation and others.  All Rights Reserved.
//
// The guest comparator is called directly, as libc's qsort calls its comparator.

#include "prx/libSceIcu/include/LegacyExports.hpp"

namespace {

// ICU's MIN_QSORT: below this many elements the search is linear.
constexpr std::int32_t MinQsort = 9;

}

extern "C" {

std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item, std::int32_t itemSize, std::int32_t (APS5_VABI* cmp)(const void*, const void*, const void*), const void* context) {
    std::int32_t start = 0;
    bool found = false;

    while ((limit - start) >= MinQsort) {
        const std::int32_t i = (start + limit) / 2;
        const std::int32_t diff = cmp(context, item, array + i * itemSize);
        if (diff == 0) {
            found = true;
            start = i + 1;
        } else if (diff < 0) {
            limit = i;
        } else {
            start = i;
        }
    }

    while (start < limit) {
        const std::int32_t diff = cmp(context, item, array + start * itemSize);
        if (diff == 0) {
            found = true;
        } else if (diff < 0) {
            break;
        }
        ++start;
    }
    return found ? (start - 1) : ~start;
}

}
