// Derived from icu4c/source/common/cstring.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_ebcdictolower.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include <cstdint>

namespace {

constexpr std::uint8_t CaseOffset = 0x40;

bool Uppercase(std::uint8_t value) {
    return (0xc1 <= value && value <= 0xc9) || (0xd1 <= value && value <= 0xd9) || (0xe2 <= value && value <= 0xe9);
}

}

extern "C" {

char APS5_VABI uprv_ebcdictolower_nid_postfix(char value) {
    const auto byte = static_cast<std::uint8_t>(value);
    return static_cast<char>(Uppercase(byte) ? byte - CaseOffset : byte);
}

}
