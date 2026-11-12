/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
 * Copyright (C) 2017 Mozilla Foundation. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Derived from Source/WTF/wtf/text/StringBuilderJSON.cpp (WebKit c55364d53f97)

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;

constexpr unsigned MaxLength = 0x7FFFFFFF;

constexpr unsigned char EscapedFormsForJSON[0x100] = {
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'b', 't', 'n', 'u', 'f', 'r', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',
    0,   0,  '\"', 0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,  '\\', 0,   0,   0,
};

char LowerNibbleToLowercaseHex(unsigned value) {
    return "0123456789abcdef"[value & 15];
}

template<typename OutputCharacterType, typename InputCharacterType>
void AppendQuotedJSONStringInternal(OutputCharacterType*& output, const InputCharacterType* input, unsigned length) {
    for (auto* end = input + length; input != end; ++input) {
        unsigned character = *input;
        if (character <= 0xFF) {
            auto escaped = EscapedFormsForJSON[character];
            if (!escaped) {
                *output++ = static_cast<OutputCharacterType>(character);
                continue;
            }
            *output++ = '\\';
            *output++ = escaped;
            if (escaped == 'u') {
                *output++ = '0';
                *output++ = '0';
                *output++ = LowerNibbleToLowercaseHex(character >> 4);
                *output++ = LowerNibbleToLowercaseHex(character);
            }
            continue;
        }
        if (character < 0xD800 || character > 0xDFFF) {
            *output++ = static_cast<OutputCharacterType>(character);
            continue;
        }
        auto next = input + 1;
        if (character <= 0xDBFF && next != end && *next >= 0xDC00 && *next <= 0xDFFF) {
            *output++ = static_cast<OutputCharacterType>(character);
            *output++ = static_cast<OutputCharacterType>(*next);
            ++input;
            continue;
        }
        unsigned upper = character >> 8;
        unsigned lower = character & 0xFF;
        *output++ = '\\';
        *output++ = 'u';
        *output++ = LowerNibbleToLowercaseHex(upper >> 4);
        *output++ = LowerNibbleToLowercaseHex(upper);
        *output++ = LowerNibbleToLowercaseHex(lower >> 4);
        *output++ = LowerNibbleToLowercaseHex(lower);
    }
}

unsigned RoundUpToPowerOfTwo(unsigned value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(StringBuilder* self, const String& string) {
    if (!self) throw std::invalid_argument("null WTF::StringBuilder");
    auto& builder = *self;
    if (builder.overflowed) return;
    const auto* impl = string.impl;
    unsigned stringLength = impl ? impl->length : 0;
    bool stringIs8Bit = !impl || (impl->hashAndFlags & 4);
    std::uint64_t maximumCapacityRequired = std::uint64_t(static_cast<unsigned>(builder.length)) + 2 + std::uint64_t(stringLength) * 6;
    if (maximumCapacityRequired > 0xFFFFFFFFULL) return NKWebKit::WtfAbi::DidOverflow(builder);
    unsigned allocationSize = static_cast<unsigned>(maximumCapacityRequired);
    allocationSize = std::max(allocationSize, RoundUpToPowerOfTwo(allocationSize));
    if (allocationSize > MaxLength) return NKWebKit::WtfAbi::DidOverflow(builder);
    if (builder.is8Bit && !stringIs8Bit) NKWebKit::WtfAbi::AllocateBufferUpConvert(builder, builder.characters8, allocationSize);
    else NKWebKit::WtfAbi::ReserveCapacity(builder, allocationSize);
    if (builder.overflowed) return;
    if (builder.is8Bit) {
        auto* output = builder.characters8 + static_cast<unsigned>(builder.length);
        *output++ = '"';
        AppendQuotedJSONStringInternal(output, impl ? static_cast<const unsigned char*>(impl->data) : nullptr, stringLength);
        *output++ = '"';
        builder.length = static_cast<std::int32_t>(output - builder.characters8);
    } else {
        auto* output = builder.characters16 + static_cast<unsigned>(builder.length);
        *output++ = '"';
        if (stringIs8Bit) AppendQuotedJSONStringInternal(output, impl ? static_cast<const unsigned char*>(impl->data) : nullptr, stringLength);
        else AppendQuotedJSONStringInternal(output, static_cast<const char16_t*>(impl->data), stringLength);
        *output++ = '"';
        builder.length = static_cast<std::int32_t>(output - builder.characters16);
    }
}

}
