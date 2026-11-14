// Derived from icu4c/source/common/uinvchar.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2010, International Business Machines
//   Corporation and others.  All Rights Reserved.
//

#include "prx/libSceIcu/include/LegacyExports.hpp"

namespace {

constexpr std::uint32_t InvariantChars[4] = {0xfffffbffu, 0xffffffe5u, 0x87fffffeu, 0x87fffffeu};

bool IsInvariant(std::uint32_t c) {
    return c <= 0x7f && (InvariantChars[c >> 5] & (1u << (c & 0x1f))) != 0;
}

}

extern "C" {

std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* s, std::int32_t length) {
    for (;;) {
        std::uint8_t c;
        if (length < 0) {
            c = static_cast<std::uint8_t>(*s++);
            if (c == 0) break;
        } else {
            if (length == 0) break;
            --length;
            c = static_cast<std::uint8_t>(*s++);
            if (c == 0) continue;
        }
        if (!IsInvariant(c)) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* s, std::int32_t length) {
    for (;;) {
        std::uint32_t c;
        if (length < 0) {
            c = *s++;
            if (c == 0) break;
        } else {
            if (length == 0) break;
            --length;
            c = *s++;
        }
        if (!IsInvariant(c)) return 0;
    }
    return 1;
}

}
