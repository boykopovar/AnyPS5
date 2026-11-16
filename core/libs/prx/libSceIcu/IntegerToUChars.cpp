// Derived from icu4c/source/common/ustrfmt.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_itou.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustrfmt.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2001-2006, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

constexpr char16_t DigitZero = 0x0030;
constexpr std::uint32_t LetterOffset = 7;

}

extern "C" {

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth) {
    if (buffer == nullptr || radix == 0) NotImplemented_nid_no_patch(__func__);
    std::int32_t length = 0;
    do {
        const std::uint32_t digit = value % radix;
        buffer[length++] = static_cast<char16_t>(digit <= 9 ? DigitZero + digit : DigitZero + digit + LetterOffset);
        value /= radix;
    } while (value != 0 && length < capacity);
    while (length < minimumWidth) buffer[length++] = DigitZero;
    if (length < capacity) buffer[length] = 0;
    for (std::int32_t index = 0; index < length / 2; ++index) {
        const char16_t swapped = buffer[length - 1 - index];
        buffer[length - 1 - index] = buffer[index];
        buffer[index] = swapped;
    }
    return length;
}

}
