// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/SharedBuffer.cpp
/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
 */
// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/SharedBuffer.h
/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
 */
// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/Vector.h
/*
 *  Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::DataSegment;
using NKWebKit::WebCoreAbi::DataSegmentVector;
using NKWebKit::WebCoreAbi::DataSegmentVectorEntry;
using NKWebKit::WebCoreAbi::SharedBuffer;
using NKWebKit::WebCoreAbi::SharedBufferRef;
using CharVector = NKWebKit::WtfAbi::Vector<char>;

constexpr signed char ValuelessIndex = -1;
constexpr signed char VectorIndex = 0;
constexpr signed char MappedFileIndex = 1;
constexpr std::size_t MinimumCapacity = 16;
constexpr unsigned InlineCapacity = 1;

template <typename T>
T& Checked(T* pointer, const char* type) {
    if (!pointer) throw std::invalid_argument(std::string("null ") + type);
    return *pointer;
}

const char* SegmentData(const DataSegment& segment) {
    if (segment.index == VectorIndex) return segment.storage.vector.buffer;
    if (segment.index == MappedFileIndex) return static_cast<const char*>(segment.storage.mappedFile.data);
    throw std::invalid_argument("WebCore::SharedBuffer::DataSegment holds no alternative");
}

std::size_t SegmentSize(const DataSegment& segment) {
    if (segment.index == VectorIndex) return segment.storage.vector.size;
    if (segment.index == MappedFileIndex) return segment.storage.mappedFile.size;
    throw std::invalid_argument("WebCore::SharedBuffer::DataSegment holds no alternative");
}

void RefSegment(DataSegment& segment) {
    segment.refCount.fetch_add(1);
}

void DerefSegment(DataSegment* segment) {
    if (!segment || segment->refCount.fetch_sub(1) != 1) return;
    if (segment->index == VectorIndex) segment->storage.vector.~Vector();
    else if (segment->index == MappedFileIndex) _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(&segment->storage.mappedFile);
    else if (segment->index != ValuelessIndex) throw std::invalid_argument("WebCore::SharedBuffer::DataSegment holds an unknown alternative");
    _ZN3WTF8fastFreeEPv(segment);
}

void ShrinkToFit(CharVector& vector) {
    if (vector.size >= vector.capacity) return;
    if (!vector.size) {
        _ZN3WTF8fastFreeEPv(std::exchange(vector.buffer, nullptr));
        vector.capacity = 0;
        return;
    }
    vector.buffer = static_cast<char*>(_ZN3WTF11fastReallocEPvm(vector.buffer, vector.size));
    vector.capacity = vector.size;
}

DataSegment* CreateSegment(CharVector&& data) {
    ShrinkToFit(data);
    auto* segment = new (_ZN3WTF10fastMallocEm(sizeof(DataSegment))) DataSegment;
    segment->refCount.store(1);
    new (&segment->storage.vector) CharVector(std::move(data));
    segment->index = VectorIndex;
    return segment;
}

CharVector CopyBytes(const char* data, std::size_t length) {
    if (length > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF::Vector<char> capacity exceeds UINT_MAX");
    if (!length) return {};
    if (!data) throw std::invalid_argument("null WebCore::SharedBuffer data with a nonzero length");
    CharVector vector(static_cast<unsigned>(std::max(length, MinimumCapacity)));
    std::memcpy(vector.buffer, data, length);
    vector.size = static_cast<unsigned>(length);
    return vector;
}

void Reserve(DataSegmentVector& segments, std::size_t capacity) {
    if (capacity <= segments.capacity) return;
    if (capacity > std::numeric_limits<unsigned>::max() / sizeof(DataSegmentVectorEntry)) throw std::length_error("WebCore::SharedBuffer segment count exceeds the WTF::Vector limit");
    auto* buffer = static_cast<DataSegmentVectorEntry*>(_ZN3WTF10fastMallocEm(capacity * sizeof(DataSegmentVectorEntry)));
    std::copy_n(segments.buffer, segments.size, buffer);
    if (segments.buffer != segments.inlineBuffer) _ZN3WTF8fastFreeEPv(segments.buffer);
    segments.buffer = buffer;
    segments.capacity = static_cast<unsigned>(capacity);
}

void ClearSegments(DataSegmentVector& segments) {
    for (unsigned i = 0; i < segments.size; ++i) DerefSegment(segments.buffer[i].segment);
    segments.size = 0;
    if (segments.buffer != segments.inlineBuffer) _ZN3WTF8fastFreeEPv(segments.buffer);
    segments.buffer = segments.inlineBuffer;
    segments.capacity = InlineCapacity;
}

void Initialize(SharedBuffer* self) {
    auto& buffer = Checked(self, "WebCore::SharedBuffer");
    buffer.refCount = 1;
    buffer.size = 0;
    buffer.segments.buffer = buffer.segments.inlineBuffer;
    buffer.segments.capacity = InlineCapacity;
    buffer.segments.size = 0;
}

void AppendSegment(SharedBuffer& buffer, CharVector&& data) {
    auto& segments = buffer.segments;
    if (segments.size == segments.capacity) Reserve(segments, std::max<std::size_t>(std::size_t(segments.size) + 1, std::max<std::size_t>(MinimumCapacity, segments.capacity + segments.capacity / 4 + 1)));
    const std::size_t length = data.size;
    DataSegment* segment = CreateSegment(std::move(data));
    segments.buffer[segments.size] = {buffer.size, segment};
    ++segments.size;
    buffer.size += length;
}

void Construct(SharedBuffer* self, CharVector&& data) {
    Initialize(self);
    AppendSegment(*self, std::move(data));
}

SharedBufferRef Create(CharVector&& data) {
    auto* buffer = static_cast<SharedBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SharedBuffer)));
    Construct(buffer, std::move(data));
    return SharedBufferRef(buffer);
}

void Combine(const SharedBuffer& constBuffer) {
    auto& buffer = const_cast<SharedBuffer&>(constBuffer);
    if (buffer.segments.size <= 1) return;
    if (buffer.size > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF::Vector<char> capacity exceeds UINT_MAX");
    CharVector combined(static_cast<unsigned>(buffer.size));
    for (unsigned i = 0; i < buffer.segments.size; ++i) {
        const auto& segment = *buffer.segments.buffer[i].segment;
        const std::size_t size = SegmentSize(segment);
        if (size > combined.capacity - combined.size) throw std::length_error("WebCore::SharedBuffer segments exceed its size");
        if (size) std::memcpy(combined.buffer + combined.size, SegmentData(segment), size);
        combined.size += static_cast<unsigned>(size);
    }
    ClearSegments(buffer.segments);
    DataSegment* segment = CreateSegment(std::move(combined));
    buffer.segments.buffer[0] = {0, segment};
    buffer.segments.size = 1;
}

const char* Data(const SharedBuffer& buffer) {
    if (!buffer.segments.size) return nullptr;
    Combine(buffer);
    return SegmentData(*buffer.segments.buffer[0].segment);
}

void DerefBuffer(SharedBuffer* buffer) {
    if (!buffer || --buffer->refCount) return;
    ClearSegments(buffer->segments);
    _ZN3WTF8fastFreeEPv(buffer);
}

}  // namespace

namespace NKWebKit::WebCoreAbi {

SharedBufferRef::~SharedBufferRef() {
    DerefBuffer(ptr);
}

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEv() {
    auto* buffer = static_cast<SharedBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SharedBuffer)));
    Initialize(buffer);
    return SharedBufferRef(buffer);
}

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEPKcm(const char* data, std::size_t size) {
    return Create(CopyBytes(data, size));
}

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEPKhm(const unsigned char* data, std::size_t size) {
    return Create(CopyBytes(reinterpret_cast<const char*>(data), size));
}

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(CharVector* vector) {
    return Create(CharVector(std::move(Checked(vector, "WTF::Vector<char>"))));
}

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WtfAbi::Vector<unsigned char>* vector) {
    const auto& source = Checked(vector, "WTF::Vector<uint8_t>");
    return Create(CopyBytes(reinterpret_cast<const char*>(source.buffer), source.size));
}

void APS5_VABI _ZN7WebCore12SharedBufferC1Ev(SharedBuffer* self) {
    Initialize(self);
}

void APS5_VABI _ZN7WebCore12SharedBufferC2Ev(SharedBuffer* self) {
    Initialize(self);
}

void APS5_VABI _ZN7WebCore12SharedBufferC1EPKcm(SharedBuffer* self, const char* data, std::size_t size) {
    Construct(self, CopyBytes(data, size));
}

void APS5_VABI _ZN7WebCore12SharedBufferC2EPKcm(SharedBuffer* self, const char* data, std::size_t size) {
    Construct(self, CopyBytes(data, size));
}

void APS5_VABI _ZN7WebCore12SharedBufferC1EPKhm(SharedBuffer* self, const unsigned char* data, std::size_t size) {
    Construct(self, CopyBytes(reinterpret_cast<const char*>(data), size));
}

void APS5_VABI _ZN7WebCore12SharedBufferC2EPKhm(SharedBuffer* self, const unsigned char* data, std::size_t size) {
    Construct(self, CopyBytes(reinterpret_cast<const char*>(data), size));
}

void APS5_VABI _ZN7WebCore12SharedBufferC1EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(SharedBuffer* self, CharVector* vector) {
    Construct(self, CharVector(std::move(Checked(vector, "WTF::Vector<char>"))));
}

void APS5_VABI _ZN7WebCore12SharedBufferC2EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(SharedBuffer* self, CharVector* vector) {
    Construct(self, CharVector(std::move(Checked(vector, "WTF::Vector<char>"))));
}

void APS5_VABI _ZN7WebCore12SharedBufferD1Ev(SharedBuffer* self) {
    ClearSegments(Checked(self, "WebCore::SharedBuffer").segments);
}

void APS5_VABI _ZN7WebCore12SharedBufferD2Ev(SharedBuffer* self) {
    ClearSegments(Checked(self, "WebCore::SharedBuffer").segments);
}

void APS5_VABI _ZN7WebCore12SharedBuffer6appendERKS0_(SharedBuffer* self, const SharedBuffer* data) {
    auto& buffer = Checked(self, "WebCore::SharedBuffer");
    const auto& source = Checked(data, "WebCore::SharedBuffer");
    const unsigned count = source.segments.size;
    Reserve(buffer.segments, std::size_t(buffer.segments.size) + count);
    for (unsigned i = 0; i < count; ++i) {
        auto& segment = *source.segments.buffer[i].segment;
        RefSegment(segment);
        buffer.segments.buffer[buffer.segments.size++] = {buffer.size, &segment};
        buffer.size += SegmentSize(segment);
    }
}

void APS5_VABI _ZN7WebCore12SharedBuffer6appendEPKcm(SharedBuffer* self, const char* data, std::size_t length) {
    auto& buffer = Checked(self, "WebCore::SharedBuffer");
    AppendSegment(buffer, CopyBytes(data, length));
}

void APS5_VABI _ZN7WebCore12SharedBuffer6appendEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(SharedBuffer* self, CharVector* data) {
    auto& buffer = Checked(self, "WebCore::SharedBuffer");
    AppendSegment(buffer, CharVector(std::move(Checked(data, "WTF::Vector<char>"))));
}

void APS5_VABI _ZN7WebCore12SharedBuffer5clearEv(SharedBuffer* self) {
    auto& buffer = Checked(self, "WebCore::SharedBuffer");
    buffer.size = 0;
    ClearSegments(buffer.segments);
}

void APS5_VABI _ZNK7WebCore12SharedBuffer21combineIntoOneSegmentEv(const SharedBuffer* self) {
    Combine(Checked(self, "WebCore::SharedBuffer"));
}

SharedBufferRef APS5_VABI _ZNK7WebCore12SharedBuffer4copyEv(const SharedBuffer* self) {
    const auto& source = Checked(self, "WebCore::SharedBuffer");
    auto clone = _ZN7WebCore12SharedBuffer6createEv();
    clone.ptr->size = source.size;
    Reserve(clone.ptr->segments, source.segments.size);
    for (unsigned i = 0; i < source.segments.size; ++i) {
        RefSegment(*source.segments.buffer[i].segment);
        clone.ptr->segments.buffer[clone.ptr->segments.size++] = source.segments.buffer[i];
    }
    return clone;
}

const char* APS5_VABI _ZNK7WebCore12SharedBuffer4dataEv(const SharedBuffer* self) {
    return Data(Checked(self, "WebCore::SharedBuffer"));
}

const unsigned char* APS5_VABI _ZNK7WebCore12SharedBuffer14dataAsUInt8PtrEv(const SharedBuffer* self) {
    return reinterpret_cast<const unsigned char*>(Data(Checked(self, "WebCore::SharedBuffer")));
}

std::size_t APS5_VABI _ZNK7WebCore12SharedBuffer4sizeEv(const SharedBuffer* self) {
    return Checked(self, "WebCore::SharedBuffer").size;
}

bool APS5_VABI _ZNK7WebCore12SharedBuffer7isEmptyEv(const SharedBuffer* self) {
    return !Checked(self, "WebCore::SharedBuffer").size;
}

const DataSegmentVectorEntry* APS5_VABI _ZNK7WebCore12SharedBuffer5beginEv(const SharedBuffer* self) {
    return Checked(self, "WebCore::SharedBuffer").segments.buffer;
}

const DataSegmentVectorEntry* APS5_VABI _ZNK7WebCore12SharedBuffer3endEv(const SharedBuffer* self) {
    const auto& segments = Checked(self, "WebCore::SharedBuffer").segments;
    return segments.buffer + segments.size;
}

const char* APS5_VABI _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(const DataSegment* self) {
    return SegmentData(Checked(self, "WebCore::SharedBuffer::DataSegment"));
}

std::size_t APS5_VABI _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(const DataSegment* self) {
    return SegmentSize(Checked(self, "WebCore::SharedBuffer::DataSegment"));
}

}
