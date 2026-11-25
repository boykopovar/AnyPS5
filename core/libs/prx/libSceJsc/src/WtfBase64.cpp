// Derived from Source/WTF/wtf/text/Base64.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
   Copyright (C) 2007, 2008, 2013, 2016 Apple Inc. All rights reserved.
   Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

   This code is based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.
*/

#include "prx/libSceJsc/include/WtfBase64Exports.hpp"
#include "prx/libSceJsc/src/WtfFastMalloc.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

using Jsc::WtfAbi::CharVector;
using Jsc::WtfAbi::StringImplFlag8Bit;

constexpr char NonAlphabet = -1;
constexpr unsigned VectorMinCapacity = 16;

const char Base64URLDecMap[128] = {
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, 0x3E, NonAlphabet, NonAlphabet,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet,
    NonAlphabet, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, 0x3F,
    NonAlphabet, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet, NonAlphabet
};

template<typename T>
bool DecodeInternal(const T* data, unsigned length, std::vector<std::uint8_t>& out) {
    out.assign(length, 0);

    unsigned equalsSignCount = 0;
    unsigned outLength = 0;
    bool hadError = false;
    for (unsigned idx = 0; idx < length; ++idx) {
        const unsigned ch = data[idx];
        if (ch == '=') {
            ++equalsSignCount;
        } else {
            const char decodedCharacter = ch < sizeof(Base64URLDecMap) ? Base64URLDecMap[ch] : NonAlphabet;
            if (decodedCharacter == NonAlphabet || equalsSignCount) {
                hadError = true;
                break;
            }
            out[outLength++] = static_cast<std::uint8_t>(decodedCharacter);
        }
    }

    out.resize(outLength);
    if (hadError) return false;
    if (!outLength) return !equalsSignCount;
    if ((outLength % 4) == 1) return false;

    outLength -= (outLength + 3) / 4;
    if (!outLength) return false;

    unsigned sidx = 0;
    unsigned didx = 0;
    if (outLength > 1) {
        while (didx < outLength - 2) {
            out[didx] = ((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003);
            out[didx + 1] = ((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017);
            out[didx + 2] = ((out[sidx + 2] << 6) & 255) | (out[sidx + 3] & 077);
            sidx += 4;
            didx += 3;
        }
    }

    if (didx < outLength) out[didx] = ((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003);
    if (++didx < outLength) out[didx] = ((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017);

    out.resize(outLength);
    return true;
}

void StoreGuestVector(CharVector& vector, unsigned grownLength, const std::vector<std::uint8_t>& bytes) {
    const auto* allocator = Jsc::WtfAbi::RegisteredFastMalloc();
    if ((vector.buffer || grownLength) && (!allocator || !allocator->allocate || !allocator->deallocate))
        throw std::runtime_error("WTF::base64URLDecode: Vector storage requires libSceNKWebKit fastMalloc");

    if (vector.buffer) allocator->deallocate(vector.buffer);
    vector.buffer = nullptr;
    vector.capacity = 0;
    vector.size = 0;
    if (!grownLength) return;

    const unsigned capacity = std::max(grownLength, VectorMinCapacity);
    auto* buffer = static_cast<std::uint8_t*>(allocator->allocate(capacity));
    if (!buffer) throw std::bad_alloc();
    if (!bytes.empty()) std::memcpy(buffer, bytes.data(), bytes.size());
    vector.buffer = buffer;
    vector.capacity = capacity;
    vector.size = static_cast<unsigned>(bytes.size());
}

}

extern "C" {

bool APS5_VABI _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(const Jsc::WtfAbi::String* in, Jsc::WtfAbi::SignedOrUnsignedCharVectorAdapter out) {
    if (!in) throw std::invalid_argument("WTF::base64URLDecode requires a string");
    if (!out.vector) throw std::invalid_argument("WTF::base64URLDecode requires a vector");

    const auto* impl = in->impl;
    const unsigned length = impl ? impl->length : 0;
    if (length && !impl->data) throw std::invalid_argument("WTF::base64URLDecode: string has no characters");

    std::vector<std::uint8_t> bytes;
    bool decoded = true;
    if (length) {
        if (impl->hashAndFlags & StringImplFlag8Bit)
            decoded = DecodeInternal(static_cast<const std::uint8_t*>(impl->data), length, bytes);
        else
            decoded = DecodeInternal(static_cast<const char16_t*>(impl->data), length, bytes);
    }

    StoreGuestVector(*out.vector, length, bytes);
    return decoded;
}

}
