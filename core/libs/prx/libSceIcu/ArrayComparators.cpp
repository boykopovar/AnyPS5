// Derived from icu4c/source/common/uarrsort.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2013, International Business Machines
//   Corporation and others.  All Rights Reserved.

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

std::int32_t APS5_VABI uprv_uint16Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    return static_cast<std::int32_t>(*static_cast<const std::uint16_t*>(left)) - static_cast<std::int32_t>(*static_cast<const std::uint16_t*>(right));
}

}
