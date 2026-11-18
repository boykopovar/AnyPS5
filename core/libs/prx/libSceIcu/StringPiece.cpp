// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2013, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2004 and onwards Google Inc.
//
// Author: wilsonh@google.com (Wilson Hsieh)
//
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/stringpiece.cpp

#include "prx/libSceIcu/include/StringPiece.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

void Read(void* output, const void* input, std::size_t count) {
    if (!GuestCopyIn_nid_no_patch(output, input, count)) throw std::invalid_argument("ICU StringPiece: unreadable input");
}

void ReadBytes(void* output, const char* data, std::size_t offset, std::size_t count) {
    const auto base = reinterpret_cast<std::uintptr_t>(data);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - base) throw std::invalid_argument("ICU StringPiece: address overflow");
    Read(output, reinterpret_cast<const void*>(base + offset), count);
}

}

extern "C" std::int8_t APS5_VABI _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(const Icu::StringPiece* left, const Icu::StringPiece* right) {
    Icu::StringPiece a{}, b{};
    constexpr auto fields = offsetof(Icu::StringPiece, length) + sizeof(a.length);
    Read(&a, left, fields);
    Read(&b, right, fields);
    if (a.length != b.length) return 0;
    if (a.length == 0) return 1;
    if (a.length < 0) throw std::invalid_argument("ICU StringPiece: negative length");

    const auto last = static_cast<std::size_t>(a.length - 1);
    char lastA, lastB;
    ReadBytes(&lastA, a.data, last, 1);
    ReadBytes(&lastB, b.data, last, 1);
    if (lastA != lastB) return 0;
    std::array<char, 1024> first, second;
    for (std::size_t offset = 0; offset < last;) {
        const auto count = std::min(first.size(), last - offset);
        ReadBytes(first.data(), a.data, offset, count);
        ReadBytes(second.data(), b.data, offset, count);
        if (std::memcmp(first.data(), second.data(), count) != 0) return 0;
        offset += count;
    }
    return 1;
}
