/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/HexNumber.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/HexNumber.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfHelperExports.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::HexConversionMode;
using Jsc::WtfAbi::LChar;

const LChar* HexDigitsForMode(HexConversionMode mode) {
    static const LChar lowercaseHexDigits[17] = "0123456789abcdef";
    static const LChar uppercaseHexDigits[17] = "0123456789ABCDEF";
    return mode == Jsc::WtfAbi::Lowercase ? lowercaseHexDigits : uppercaseHexDigits;
}

unsigned HexDigitCount(std::uintmax_t number) {
    unsigned digits = 0;
    do {
        ++digits;
        number >>= 4;
    } while (number);
    return digits;
}

} // namespace

extern "C" {

Jsc::WtfAbi::HexResult APS5_VABI _ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE(LChar* buffer, unsigned bufferSize, std::uintmax_t number, unsigned minimumDigits, HexConversionMode mode) {
    if (!buffer) throw std::invalid_argument("WTF::Internal::appendHex: null buffer");
    if (HexDigitCount(number) > bufferSize) throw std::invalid_argument("WTF::Internal::appendHex: buffer too small for the number");
    auto end = buffer + bufferSize;
    auto start = end;
    auto hexDigits = HexDigitsForMode(mode);
    do {
        *--start = hexDigits[number & 0xF];
        number >>= 4;
    } while (number);
    auto startWithLeadingZeros = end - std::min(minimumDigits, bufferSize);
    if (start > startWithLeadingZeros) {
        std::memset(startWithLeadingZeros, '0', start - startWithLeadingZeros);
        start = startWithLeadingZeros;
    }
    return {start, static_cast<unsigned>(end - start)};
}

}
