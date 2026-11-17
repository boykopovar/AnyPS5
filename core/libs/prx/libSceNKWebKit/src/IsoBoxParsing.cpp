// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOBox.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/ArrayBufferView.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/DataView.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/ArrayBuffer.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/Vector.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/ArrayBufferView.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/DataView.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/runtime/ArrayBuffer.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 *  Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/Vector.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/src/IsoBoxParsing.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace NKWebKit::IsoParse {

namespace {

template<typename T> T Load(const void* base, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(T));
    return value;
}

bool BufferIsDetached(const void* buffer) {
    return !Load<const void*>(buffer, ArrayBufferDataOffset);
}

bool ViewIsDetached(const DataView& view) {
    return !view.buffer || BufferIsDetached(view.buffer);
}

const unsigned char* ViewBase(const DataView& view) {
    return ViewIsDetached(view) ? nullptr : static_cast<const unsigned char*>(view.baseAddress);
}

template<typename T> T FlipBytes(T value) {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else {
        unsigned char bytes[sizeof(T)];
        std::memcpy(bytes, &value, sizeof(T));
        std::reverse(bytes, bytes + sizeof(T));
        std::memcpy(&value, bytes, sizeof(T));
        return value;
    }
}

template<typename T> bool CheckedRead(const DataView& view, unsigned& offset, T& value) {
    std::size_t actualOffset = offset;
    if (actualOffset + sizeof(T) > ViewByteLength(view))
        return false;
    T raw;
    std::memcpy(&raw, ViewBase(view) + actualOffset, sizeof(T));
    actualOffset += sizeof(T);
    if (actualOffset > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("WebCore::ISOBox: read offset exceeds 32 bits");
    offset = static_cast<unsigned>(actualOffset);
    value = FlipBytes(raw);
    return true;
}

void ReserveCapacity(ByteVector& vector, unsigned capacity) {
    if (capacity <= vector.capacity)
        return;
    auto* buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(capacity));
    if (vector.size)
        std::memcpy(buffer, vector.buffer, vector.size);
    if (vector.buffer)
        _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = buffer;
    vector.capacity = capacity;
}

}  // namespace

const DataView& CheckedView(const DataView* view, const char* function) {
    if (!view) throw std::invalid_argument(std::string(function) + " requires a DataView");
    return *view;
}

std::size_t BufferByteLength(const void* buffer) {
    if (Load<bool>(buffer, ArrayBufferHasMaxOffset)) {
        if (const void* shared = Load<const void*>(buffer, ArrayBufferSharedOffset))
            return Load<std::size_t>(shared, SharedContentsSizeOffset);
    }
    return Load<std::size_t>(buffer, ArrayBufferSizeOffset);
}

std::size_t ViewByteLength(const DataView& view) {
    if (ViewIsDetached(view))
        return 0;
    if (!(view.flags & (ViewResizableNonShared | ViewGrowableShared)))
        return view.byteLength;
    std::size_t bufferByteLength = BufferByteLength(view.buffer);
    std::size_t byteOffsetStart = view.byteOffset;
    std::size_t byteOffsetEnd = 0;
    if (view.flags & ViewAutoLength)
        byteOffsetEnd = bufferByteLength;
    else
        byteOffsetEnd = byteOffsetStart + view.byteLength;
    if (!(byteOffsetStart > bufferByteLength || byteOffsetEnd > bufferByteLength))
        return 0;
    if (!(view.flags & ViewAutoLength))
        return view.byteLength;
    return bufferByteLength - byteOffsetStart;
}

const void* ViewBuffer(const DataView& view) {
    return ViewIsDetached(view) ? nullptr : view.buffer;
}

std::vector<std::uint8_t> Slice(const void* buffer, double begin, double end) {
    std::size_t length = BufferByteLength(buffer);
    auto clamp = [length](double index) -> std::size_t {
        if (index < 0)
            index = static_cast<double>(length) + index;
        if (!(index >= 0))
            return 0;
        if (index > static_cast<double>(length))
            return length;
        return static_cast<std::size_t>(index);
    };
    std::size_t first = clamp(begin);
    std::size_t last = clamp(end);
    std::size_t size = first <= last ? last - first : 0;
    const auto* data = Load<const unsigned char*>(buffer, ArrayBufferDataOffset);
    return std::vector<std::uint8_t>(data + first, data + first + size);
}

bool ReadU8(const DataView& view, unsigned& offset, std::int8_t& value) {
    return CheckedRead(view, offset, value);
}

bool ReadU32(const DataView& view, unsigned& offset, std::uint32_t& value) {
    return CheckedRead(view, offset, value);
}

bool ReadU64(const DataView& view, unsigned& offset, std::uint64_t& value) {
    return CheckedRead(view, offset, value);
}

void Resize(ByteVector& vector, unsigned size) {
    if (size > vector.capacity) {
        std::size_t grown = std::max<std::size_t>(16, std::size_t(vector.capacity) + vector.capacity / 4 + 1);
        ReserveCapacity(vector, static_cast<unsigned>(std::max<std::size_t>(size, grown)));
    }
    if (size > vector.size)
        std::memset(vector.buffer + vector.size, 0, size - vector.size);
    vector.size = size;
}

void ReserveInitialCapacity(ByteVector& vector, unsigned capacity) {
    ReserveCapacity(vector, capacity);
}

void Assign(ByteVector& target, const ByteVector& source) {
    if (&target == &source)
        return;
    if (target.size > source.size)
        target.size = source.size;
    else if (source.size > target.capacity) {
        Destroy(target);
        ReserveCapacity(target, source.size);
    }
    if (source.size)
        std::memcpy(target.buffer, source.buffer, source.size);
    target.size = source.size;
}

void MoveAssign(ByteVector& target, ByteVector&& source) {
    if (&target == &source)
        return;
    ByteVector moved(std::move(source));
    target.swap(moved);
}

ByteVector Copy(const ByteVector& source) {
    return ByteVector(source);
}

void Destroy(ByteVector& vector) {
    if (vector.buffer)
        _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = nullptr;
    vector.capacity = 0;
    vector.size = 0;
}

void ConstructBox(Box& box, const Vtable* vtable) {
    box.vtable = reinterpret_cast<const void* const*>(vtable);
    box.size = 0;
    box.boxType = 0;
    new (&box.extendedType) ByteVector();
}

void CopyBox(Box& box, const Box& other, const Vtable* vtable) {
    box.vtable = reinterpret_cast<const void* const*>(vtable);
    box.size = other.size;
    box.boxType = other.boxType;
    new (&box.extendedType) ByteVector(other.extendedType);
}

void AssignBox(Box& box, const Box& other) {
    box.size = other.size;
    box.boxType = other.boxType;
    Assign(box.extendedType, other.extendedType);
}

void DestroyBox(Box& box, const Vtable* vtable) {
    box.vtable = reinterpret_cast<const void* const*>(vtable);
    Destroy(box.extendedType);
}

void ConstructFullBox(FullBox& box, const Vtable* vtable) {
    ConstructBox(box.box, vtable);
    box.version = 0;
    box.flags = 0;
}

void CopyFullBox(FullBox& box, const FullBox& other, const Vtable* vtable) {
    CopyBox(box.box, other.box, vtable);
    box.version = other.version;
    box.flags = other.flags;
}

void AssignFullBox(FullBox& box, const FullBox& other) {
    AssignBox(box.box, other.box);
    box.version = other.version;
    box.flags = other.flags;
}

bool ParseBox(Box& box, const DataView& view, unsigned& offset) {
    unsigned maximumPossibleSize = static_cast<unsigned>(ViewByteLength(view) - offset);
    std::uint32_t value = 0;
    if (!ReadU32(view, offset, value))
        return false;
    box.size = value;
    if (!ReadU32(view, offset, value))
        return false;
    box.boxType = value;
    if (box.size == 1 && !ReadU64(view, offset, box.size))
        return false;
    if (box.size > maximumPossibleSize)
        box.size = maximumPossibleSize;
    else if (!box.size)
        box.size = maximumPossibleSize;

    if (box.boxType == FourCC("uuid")) {
        if (std::size_t(offset) + 16 > ViewByteLength(view))
            return false;
        throw std::logic_error("WebCore::ISOBox::parse: WebKit's flipBytes reaches RELEASE_ASSERT_NOT_REACHED for the 16-byte uuid extended type");
    }
    return true;
}

bool ParseVersionAndFlags(FullBox& box, const DataView& view, unsigned& offset) {
    std::uint32_t versionAndFlags = 0;
    if (!ReadU32(view, offset, versionAndFlags))
        return false;
    box.version = static_cast<std::uint8_t>(versionAndFlags >> 24);
    box.flags = versionAndFlags & 0xFFFFFF;
    return true;
}

bool ParseFullBox(FullBox& box, const DataView& view, unsigned& offset) {
    if (!ParseBox(box.box, view, offset))
        return false;
    return ParseVersionAndFlags(box, view, offset);
}

PeekResult PeekBox(const DataView& view, unsigned offset) {
    unsigned maximumPossibleSize = static_cast<unsigned>(ViewByteLength(view) - offset);
    std::uint32_t value = 0;
    if (!ReadU32(view, offset, value))
        return {0, 0, false};
    std::uint64_t size = value;
    std::uint32_t type = 0;
    if (!ReadU32(view, offset, type))
        return {0, 0, false};
    if (size == 1 && !ReadU64(view, offset, size))
        return {0, 0, false};
    if (size > maximumPossibleSize)
        size = maximumPossibleSize;
    else if (!size)
        size = maximumPossibleSize;
    return {type, size, true};
}

bool VirtualParse(Box& box, const DataView& view, unsigned& offset) {
    auto* vtable = reinterpret_cast<const Vtable*>(box.vtable);
    return vtable->parse(&box, const_cast<DataView*>(&view), &offset);
}

bool VirtualRead(Box& box, const DataView& view, unsigned& offset) {
    unsigned localOffset = offset;
    if (!VirtualParse(box, view, localOffset))
        return false;
    offset += static_cast<unsigned>(box.size);
    return true;
}

void DeleteBox(Box* box) {
    if (!box)
        return;
    reinterpret_cast<const Vtable*>(box->vtable)->deletingDestructor(box);
}

void* AllocateBox(std::size_t size) {
    return _ZN7WebCore6ISOBoxnwEm(size);
}

void FreeBox(void* box) {
    _ZN7WebCore6ISOBoxdlEPv(box);
}

}  // namespace NKWebKit::IsoParse
