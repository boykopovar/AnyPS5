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
// https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/dtoa/double-conversion.cc

#include "prx/libSceNKWebKit/include/DoubleConversionExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::DoubleConversionStringBuilder;
using NKWebKit::WtfAbi::DoubleToStringConverter;

constexpr int emitTrailingDecimalPoint = 2;
constexpr int emitTrailingZeroAfterPoint = 4;
constexpr int uniqueZero = 8;

std::string ReadSymbol(const char* symbol, std::size_t capacity) {
    std::string result;
    auto address = reinterpret_cast<std::uintptr_t>(symbol);
    for (;;) {
        char character;
        if (!GuestCopyIn_nid_no_patch(&character, reinterpret_cast<const void*>(address), sizeof(character)))
            throw std::invalid_argument("DoubleToFixed: unreadable special value symbol");
        if (!character)
            return result;
        if (result.size() == capacity)
            throw std::out_of_range("DoubleToFixed: special value symbol exceeds buffer");
        result += character;
        if (address == std::numeric_limits<std::uintptr_t>::max())
            throw std::invalid_argument("DoubleToFixed: special value address overflow");
        ++address;
    }
}

}  // namespace

extern "C" {

bool APS5_VABI
_ZNK3WTF17double_conversion23DoubleToStringConverter7ToFixedEdiPNS0_13StringBuilderE(
    const DoubleToStringConverter* self,
    double value,
    int requestedDigits,
    DoubleConversionStringBuilder* resultBuilder) {
    const bool finite = std::isfinite(value);
    if (finite) {
        if (requestedDigits > 100 || value >= 1e21 || value <= -1e21)
            return false;
        if (requestedDigits < 0)
            throw std::invalid_argument("DoubleToFixed: negative precision");
    }

    DoubleToStringConverter converter;
    if (!GuestCopyIn_nid_no_patch(&converter, self, sizeof(converter)))
        throw std::invalid_argument("DoubleToFixed: unreadable converter");
    const char* symbol = std::isnan(value) ? converter.nanSymbol : converter.infinitySymbol;
    if (!finite && !symbol)
        return false;

    DoubleConversionStringBuilder builder;
    if (!GuestCopyIn_nid_no_patch(&builder, resultBuilder, sizeof(builder)))
        throw std::invalid_argument("DoubleToFixed: unreadable builder");
    if (!builder.buffer.start || builder.buffer.length <= 0 || builder.position < 0 || builder.position >= builder.buffer.length)
        throw std::invalid_argument("DoubleToFixed: invalid builder state");
    const auto capacity = static_cast<std::size_t>(builder.buffer.length - builder.position - 1);

    std::string result;
    if (finite) {
        result = NKWebKit::WtfAbi::NumberToFixedWidthString(value, static_cast<unsigned>(requestedDigits));
        if (value == 0 && std::signbit(value) && !(converter.flags & uniqueZero))
            result.insert(result.begin(), '-');
        if (requestedDigits == 0) {
            if (converter.flags & emitTrailingDecimalPoint)
                result += '.';
            if (converter.flags & emitTrailingZeroAfterPoint)
                result += '0';
        }
    } else {
        if (value < 0)
            result += '-';
        if (result.size() > capacity)
            throw std::out_of_range("DoubleToFixed: buffer overflow");
        result += ReadSymbol(symbol, capacity - result.size());
    }
    if (result.size() > capacity)
        throw std::out_of_range("DoubleToFixed: buffer overflow");

    const auto bufferAddress = reinterpret_cast<std::uintptr_t>(builder.buffer.start);
    if (bufferAddress > std::numeric_limits<std::uintptr_t>::max() - static_cast<unsigned>(builder.position))
        throw std::invalid_argument("DoubleToFixed: buffer address overflow");
    auto* output = reinterpret_cast<void*>(bufferAddress + static_cast<unsigned>(builder.position));
    auto* position = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(resultBuilder) + offsetof(DoubleConversionStringBuilder, position));
    if (!GuestWritable_nid_no_patch(position, sizeof(builder.position)) || !GuestWritable_nid_no_patch(output, result.size()))
        throw std::invalid_argument("DoubleToFixed: unwritable output");
    builder.position += static_cast<int>(result.size());
    if (!GuestCopyOut_nid_no_patch(output, result.data(), result.size()) || !GuestCopyOut_nid_no_patch(position, &builder.position, sizeof(builder.position)))
        throw std::invalid_argument("DoubleToFixed: output copy failed");
    return true;
}

}
