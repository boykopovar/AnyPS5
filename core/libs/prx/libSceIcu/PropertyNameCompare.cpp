// Derived from icu4c/source/common/propname.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_compareASCIIPropertyNames and uprv_compareEBCDICPropertyNames.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/propname.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (c) 2002-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.
// Derived from icu4c/source/common/cstring.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the lowercasing of uprv_asciitolower and uprv_ebcdictolower.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

using NextCharacter = std::int32_t (*)(const char*);

std::uint8_t AsciiLower(std::uint8_t value) {
    return 0x41 <= value && value <= 0x5a ? static_cast<std::uint8_t>(value + 0x20) : value;
}

std::uint8_t EbcdicLower(std::uint8_t value) {
    const bool upper = (0xc1 <= value && value <= 0xc9) || (0xd1 <= value && value <= 0xd9) || (0xe2 <= value && value <= 0xe9);
    return upper ? static_cast<std::uint8_t>(value - 0x40) : value;
}

bool AsciiIgnored(std::uint8_t value) {
    return value == 0x2d || value == 0x5f || value == 0x20 || (0x09 <= value && value <= 0x0d);
}

bool EbcdicIgnored(std::uint8_t value) {
    return value == 0x60 || value == 0x6d || value == 0x40 || value == 0x05 || value == 0x15 || value == 0x25 || value == 0x0b || value == 0x0c || value == 0x0d;
}

std::int32_t NextAscii(const char* name) {
    std::int32_t index = 0;
    std::uint8_t value;
    do {
        value = static_cast<std::uint8_t>(name[index++]);
    } while (AsciiIgnored(value));
    return (index << 8) | AsciiLower(value);
}

std::int32_t NextEbcdic(const char* name) {
    std::int32_t index = 0;
    std::uint8_t value;
    do {
        value = static_cast<std::uint8_t>(name[index++]);
    } while (EbcdicIgnored(value));
    return (index << 8) | EbcdicLower(value);
}

std::int32_t Compare(const char* name1, const char* name2, NextCharacter next) {
    for (;;) {
        const std::int32_t first = next(name1);
        const std::int32_t second = next(name2);
        if (((first | second) & 0xff) == 0) return 0;
        if (first != second) {
            const std::int32_t difference = (first & 0xff) - (second & 0xff);
            if (difference != 0) return difference;
        }
        name1 += first >> 8;
        name2 += second >> 8;
    }
}

}

extern "C" {

std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* name1, const char* name2) {
    if (name1 == nullptr || name2 == nullptr) NotImplemented_nid_no_patch(__func__);
    return Compare(name1, name2, NextAscii);
}

std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char* name1, const char* name2) {
    if (name1 == nullptr || name2 == nullptr) NotImplemented_nid_no_patch(__func__);
    return Compare(name1, name2, NextEbcdic);
}

}
