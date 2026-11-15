// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/* Derived from Source/WTF/wtf/dtoa/double-conversion.cc and Source/WTF/wtf/text/WTFString.cpp (WebKit c55364d53f97) */

#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr std::size_t MaxSignificantDigits = 772;
constexpr std::size_t ConversionBufferSize = 64;
constexpr std::int64_t MaxExponent = std::numeric_limits<int>::max() / 2;
constexpr std::int64_t MaxDecimalPower = 309;
constexpr std::int64_t MinDecimalPower = -324;

template<typename TFloat>
struct ParsedNumber {
    TFloat value;
    std::size_t length;
};

template<typename TCharacter>
bool IsDigit(TCharacter character) {
    return character >= '0' && character <= '9';
}

std::int64_t CheckedExponent(std::int64_t value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) throw std::overflow_error("WTF::double_conversion: the decimal exponent overflows int, which is undefined in WebKit");
    return value;
}

template<typename TCharacter>
bool IsSpace(TCharacter character) {
    return character <= ' ' && (character == ' ' || (character <= 0xD && character >= 0x9));
}

template<typename TFloat>
TFloat Convert(std::string digits, std::int64_t exponent) {
    while (!digits.empty() && digits.back() == '0') {
        digits.pop_back();
        exponent = CheckedExponent(exponent + 1);
    }
    if (digits.empty()) return 0;
    const auto count = static_cast<std::int64_t>(digits.size());
    CheckedExponent(exponent + count);
    if (exponent + count - 1 >= MaxDecimalPower) return std::numeric_limits<TFloat>::infinity();
    if (exponent + count <= MinDecimalPower) return 0;
    digits += 'e';
    digits += std::to_string(exponent);
    TFloat value = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (result.ec == std::errc::result_out_of_range) return exponent + count > 0 ? std::numeric_limits<TFloat>::infinity() : 0;
    if (result.ec != std::errc() || result.ptr != digits.data() + digits.size()) throw std::logic_error("WTF::parseDouble: decimal conversion rejected its digits");
    return value;
}

template<typename TFloat, typename TCharacter>
class DecimalParser {
public:
    DecimalParser(const TCharacter* input, std::size_t length) : input(input), current(input), end(input + length) {}

    ParsedNumber<TFloat> Parse() {
        if (current == end) return {0, 0};
        if (*current == '+' || *current == '-') {
            negative = *current == '-';
            if (++current == end) return {0, 0};
        }
        bool leadingZero = false;
        if (*current == '0') {
            leadingZero = true;
            while (*current == '0') {
                if (++current == end) return signedZero();
            }
        }
        while (IsDigit(*current)) {
            if (digits.size() < MaxSignificantDigits) digits.push_back(static_cast<char>(*current));
            else {
                insignificantDigits = CheckedExponent(insignificantDigits + 1);
                nonzeroDigitDropped = nonzeroDigitDropped || *current != '0';
            }
            if (++current == end) return finish();
        }
        if (*current == '.') {
            if (++current == end) {
                if (digits.empty() && !leadingZero) return {0, 0};
                return finish();
            }
            if (digits.empty()) {
                while (*current == '0') {
                    if (++current == end) return signedZero();
                    exponent = CheckedExponent(exponent - 1);
                }
            }
            while (IsDigit(*current)) {
                if (digits.size() < MaxSignificantDigits) {
                    digits.push_back(static_cast<char>(*current));
                    exponent = CheckedExponent(exponent - 1);
                } else nonzeroDigitDropped = nonzeroDigitDropped || *current != '0';
                if (++current == end) return finish();
            }
        }
        if (!leadingZero && exponent == 0 && digits.empty()) return {0, 0};
        if (*current == 'e' || *current == 'E') {
            const auto* marker = current;
            if (++current == end) return finishAt(marker);
            bool negativeExponent = false;
            if (*current == '+' || *current == '-') {
                negativeExponent = *current == '-';
                if (++current == end) return finishAt(marker);
            }
            if (!IsDigit(*current)) return finishAt(marker);
            std::int64_t number = 0;
            do {
                const int digit = *current - '0';
                if (number >= MaxExponent / 10 && !(number == MaxExponent / 10 && digit <= MaxExponent % 10)) number = MaxExponent;
                else number = number * 10 + digit;
                ++current;
            } while (current != end && IsDigit(*current));
            exponent = CheckedExponent(exponent + (negativeExponent ? -number : number));
        }
        return finish();
    }

private:
    ParsedNumber<TFloat> signedZero() const {
        return {negative ? -TFloat(0) : TFloat(0), static_cast<std::size_t>(current - input)};
    }

    ParsedNumber<TFloat> finishAt(const TCharacter* position) {
        current = position;
        return finish();
    }

    ParsedNumber<TFloat> finish() {
        exponent = CheckedExponent(exponent + insignificantDigits);
        if (nonzeroDigitDropped) {
            digits.push_back('1');
            exponent = CheckedExponent(exponent - 1);
        }
        const TFloat converted = Convert<TFloat>(digits, exponent);
        return {negative ? -converted : converted, static_cast<std::size_t>(current - input)};
    }

    const TCharacter* input;
    const TCharacter* current;
    const TCharacter* end;
    std::string digits;
    std::int64_t exponent = 0;
    std::int64_t insignificantDigits = 0;
    bool nonzeroDigitDropped = false;
    bool negative = false;
};

template<typename TFloat, typename TCharacter>
TFloat StringToIeee(const TCharacter* data, std::size_t length, std::size_t* processedCount) {
    if (!processedCount) throw std::invalid_argument("WTF::double_conversion: a null processed length faults in WebKit");
    *processedCount = 0;
    if (!data && length) throw std::invalid_argument("WTF::double_conversion: null characters with a nonzero length fault in WebKit");
    const auto parsed = DecimalParser<TFloat, TCharacter>(data, length).Parse();
    *processedCount = parsed.length;
    return parsed.value;
}

double ParseDouble(const unsigned char* data, std::size_t length, std::size_t* parsedLength) {
    return StringToIeee<double>(data, length, parsedLength);
}

double ParseDouble(const char16_t* data, std::size_t length, std::size_t* parsedLength) {
    if (length <= ConversionBufferSize) {
        std::array<unsigned char, ConversionBufferSize> buffer{};
        for (std::size_t index = 0; index < length; ++index) buffer[index] = data[index] < 0x80 ? static_cast<unsigned char>(data[index]) : 0;
        return ParseDouble(buffer.data(), length, parsedLength);
    }
    if (length > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("WTF::Vector: conversion buffer capacity above UINT_MAX");
    std::vector<unsigned char> buffer(length);
    for (std::size_t index = 0; index < length; ++index) buffer[index] = data[index] < 0x80 ? static_cast<unsigned char>(data[index]) : 0;
    return ParseDouble(buffer.data(), length, parsedLength);
}

template<typename TCharacter>
double ToDouble(const TCharacter* data, std::size_t length, bool allowTrailingJunk, bool* ok, std::size_t* parsedLength) {
    if (!data && length) throw std::invalid_argument("WTF number parsing: null characters with a nonzero length fault in WebKit");
    std::size_t leadingSpaces = 0;
    while (leadingSpaces < length && IsSpace(data[leadingSpaces])) ++leadingSpaces;
    const double value = ParseDouble(data + leadingSpaces, length - leadingSpaces, parsedLength);
    if (!*parsedLength) {
        if (ok) *ok = false;
        return 0.0;
    }
    *parsedLength += leadingSpaces;
    if (ok) *ok = allowTrailingJunk || *parsedLength == length;
    return value;
}

}

extern "C" {

float APS5_VABI _ZN3WTF17charactersToFloatEPKDsmPb(const char16_t* data, std::size_t length, bool* ok) {
    std::size_t parsedLength = 0;
    return static_cast<float>(ToDouble(data, length, false, ok, &parsedLength));
}

float APS5_VABI _ZN3WTF17charactersToFloatEPKDsmRm(const char16_t* data, std::size_t length, std::size_t* parsedLength) {
    return static_cast<float>(ToDouble(data, length, true, nullptr, parsedLength));
}

float APS5_VABI _ZN3WTF17charactersToFloatEPKhmPb(const unsigned char* data, std::size_t length, bool* ok) {
    std::size_t parsedLength = 0;
    return static_cast<float>(ToDouble(data, length, false, ok, &parsedLength));
}

float APS5_VABI _ZN3WTF17charactersToFloatEPKhmRm(const unsigned char* data, std::size_t length, std::size_t* parsedLength) {
    return static_cast<float>(ToDouble(data, length, true, nullptr, parsedLength));
}

double APS5_VABI _ZN3WTF18charactersToDoubleEPKDsmPb(const char16_t* data, std::size_t length, bool* ok) {
    std::size_t parsedLength = 0;
    return ToDouble(data, length, false, ok, &parsedLength);
}

double APS5_VABI _ZN3WTF18charactersToDoubleEPKhmPb(const unsigned char* data, std::size_t length, bool* ok) {
    std::size_t parsedLength = 0;
    return ToDouble(data, length, false, ok, &parsedLength);
}

float APS5_VABI _ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKcmPm(const char* buffer, std::size_t length, std::size_t* processedCharactersCount) {
    return StringToIeee<float>(buffer, length, processedCharactersCount);
}

float APS5_VABI _ZN3WTF17double_conversion23StringToDoubleConverter13StringToFloatEPKtmPm(const std::uint16_t* buffer, std::size_t length, std::size_t* processedCharactersCount) {
    return StringToIeee<float>(buffer, length, processedCharactersCount);
}

double APS5_VABI _ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKcmPm(const char* buffer, std::size_t length, std::size_t* processedCharactersCount) {
    return StringToIeee<double>(buffer, length, processedCharactersCount);
}

double APS5_VABI _ZN3WTF17double_conversion23StringToDoubleConverter14StringToDoubleEPKtmPm(const std::uint16_t* buffer, std::size_t length, std::size_t* processedCharactersCount) {
    return StringToIeee<double>(buffer, length, processedCharactersCount);
}

}
