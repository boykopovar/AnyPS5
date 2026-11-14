// Adapted from the UComparator helpers in ICU 68.2 common/uarrsort.cpp:
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
//
// ICU's notice, kept as the upstream file has it:
//   Copyright (C) 2003-2013, International Business Machines
//   Corporation and others.  All Rights Reserved.
// ICU and its data are distributed under the Unicode licence:
//   https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/LICENSE
//
// ICU computes `*left - *right` in int32_t, which overflows for some inputs.
// That is undefined in C++, so this file does the subtraction unsigned and
// converts the result back, which is what ICU's two's-complement builds produce.

#include "prx/libSceIcu/include/LegacyExports.hpp"

extern "C" {

std::int32_t APS5_VABI uprv_int32Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    const auto l = static_cast<std::uint32_t>(*static_cast<const std::int32_t*>(left));
    const auto r = static_cast<std::uint32_t>(*static_cast<const std::int32_t*>(right));
    return static_cast<std::int32_t>(l - r);
}

std::int32_t APS5_VABI uprv_uint32Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    const auto l = *static_cast<const std::uint32_t*>(left);
    const auto r = *static_cast<const std::uint32_t*>(right);
    if (l < r) return -1;
    if (l == r) return 0;
    return 1;
}

}
