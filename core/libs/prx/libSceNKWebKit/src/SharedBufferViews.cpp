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
// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/posix/SharedBufferPOSIX.cpp
/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/SharedBufferViewExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::DataSegment;
using NKWebKit::WebCoreAbi::DataSegmentVectorEntry;
using NKWebKit::WebCoreAbi::SharedBuffer;
using NKWebKit::WebCoreAbi::SharedBufferDataView;
using NKWebKit::WebCoreAbi::SharedBufferDataViewResult;
using NKWebKit::WebCoreAbi::SharedBufferRef;
using NKWebKit::WtfAbi::String;

constexpr signed char VectorIndex = 0;
constexpr signed char MappedFileIndex = 1;
constexpr signed char ValuelessIndex = -1;
constexpr int ReadOnly = 0;
constexpr int SharedMapping = 0;

template<typename T> T& Checked(T* pointer, const char* function) {
    if (!pointer) throw std::invalid_argument(std::string(function) + " requires an object");
    return *pointer;
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

std::size_t SegmentSize(const DataSegment& segment) {
    return _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(&segment);
}

const char* SegmentData(const DataSegment& segment) {
    return _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(&segment);
}

void ConstructFromMappedFile(SharedBuffer& buffer, WTF::MappedFileData& data) {
    _ZN7WebCore12SharedBufferC2Ev(&buffer);
    buffer.size = data.size;
    auto* segment = static_cast<DataSegment*>(_ZN3WTF10fastMallocEm(sizeof(DataSegment)));
    new (segment) DataSegment;
    segment->refCount.store(1);
    segment->storage.mappedFile.data = std::exchange(data.data, nullptr);
    segment->storage.mappedFile.size = std::exchange(data.size, 0U);
    segment->index = MappedFileIndex;
    buffer.segments.buffer[0] = {0, segment};
    buffer.segments.size = 1;
}

SharedBufferRef CreateFromReadingFile(const String& filePath) {
    if (!filePath.impl || !filePath.impl->length) return SharedBufferRef(nullptr);
    const auto filename = _ZN3WTF14FileSystemImpl24fileSystemRepresentationERKNS_6StringE(&filePath);
    const char* path = filename.buffer ? reinterpret_cast<const char*>(filename.buffer + 1) : "";
    int fd = open_nid_postfix(path, ReadOnly, 0);
    if (fd == -1) return SharedBufferRef(nullptr);
    FileStat fileStat{};
    if (_fstat_nid_postfix(fd, &fileStat)) {
        close_nid_postfix(fd);
        return SharedBufferRef(nullptr);
    }
    if (fileStat.st_size < 0) {
        close_nid_postfix(fd);
        return SharedBufferRef(nullptr);
    }
    const auto bytesToRead = static_cast<std::uint64_t>(fileStat.st_size);
    if (bytesToRead > std::numeric_limits<unsigned>::max()) {
        close_nid_postfix(fd);
        throw std::length_error("WebCore::SharedBuffer::createFromReadingFile: WTF::Vector<char> capacity exceeds UINT_MAX");
    }
    NKWebKit::WtfAbi::Vector<char> buffer(static_cast<unsigned>(bytesToRead));
    buffer.size = static_cast<unsigned>(bytesToRead);
    std::uint64_t totalBytesRead = 0;
    std::int64_t bytesRead;
    while ((bytesRead = read_nid_postfix(fd, buffer.buffer + totalBytesRead, bytesToRead - totalBytesRead)) > 0)
        totalBytesRead += static_cast<std::uint64_t>(bytesRead);
    close_nid_postfix(fd);
    if (totalBytesRead != bytesToRead) return SharedBufferRef(nullptr);
    return _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&buffer);
}

void ConstructView(SharedBufferDataView* self, DataSegment** segment, std::size_t positionWithinSegment) {
    DataSegment*& source = Checked(segment, "WebCore::SharedBufferDataView::SharedBufferDataView");
    auto& view = Checked(self, "WebCore::SharedBufferDataView::SharedBufferDataView");
    Checked(source, "WebCore::SharedBufferDataView::SharedBufferDataView");
    view.positionWithinSegment = positionWithinSegment;
    view.segment = std::exchange(source, nullptr);
}

void CopyView(SharedBufferDataView* self, const SharedBufferDataView* other) {
    const auto& source = Checked(other, "WebCore::SharedBufferDataView::SharedBufferDataView");
    auto& view = Checked(self, "WebCore::SharedBufferDataView::SharedBufferDataView");
    RefSegment(Checked(source.segment, "WebCore::SharedBufferDataView::SharedBufferDataView"));
    view.positionWithinSegment = source.positionWithinSegment;
    view.segment = source.segment;
}

void DestroyView(SharedBufferDataView* self) {
    DerefSegment(std::exchange(Checked(self, "WebCore::SharedBufferDataView::~SharedBufferDataView").segment, nullptr));
}

bool Equal(const SharedBuffer& buffer, const SharedBuffer& other) {
    if (&buffer == &other) return true;
    if (buffer.size != other.size) return false;
    const DataSegmentVectorEntry* thisIterator = buffer.segments.buffer;
    const DataSegmentVectorEntry* thisEnd = thisIterator + buffer.segments.size;
    std::size_t thisOffset = 0;
    const DataSegmentVectorEntry* otherIterator = other.segments.buffer;
    const DataSegmentVectorEntry* otherEnd = otherIterator + other.segments.size;
    std::size_t otherOffset = 0;
    while (thisIterator != thisEnd && otherIterator != otherEnd) {
        const DataSegment& thisSegment = *thisIterator->segment;
        const DataSegment& otherSegment = *otherIterator->segment;
        if (&thisSegment == &otherSegment && !thisOffset && !otherOffset) {
            ++thisIterator;
            ++otherIterator;
            continue;
        }
        const std::size_t thisRemaining = SegmentSize(thisSegment) - thisOffset;
        const std::size_t otherRemaining = SegmentSize(otherSegment) - otherOffset;
        const std::size_t remaining = std::min(thisRemaining, otherRemaining);
        if (remaining && std::memcmp(SegmentData(thisSegment) + thisOffset, SegmentData(otherSegment) + otherOffset, remaining)) return false;
        thisOffset += remaining;
        otherOffset += remaining;
        if (thisOffset == SegmentSize(thisSegment)) {
            ++thisIterator;
            thisOffset = 0;
        }
        if (otherOffset == SegmentSize(otherSegment)) {
            ++otherIterator;
            otherOffset = 0;
        }
    }
    return true;
}

}  // namespace

namespace NKWebKit::WebCoreAbi {

SharedBufferDataViewResult::~SharedBufferDataViewResult() {
    DerefSegment(view.segment);
}

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer24createWithContentsOfFileERKN3WTF6StringE(const String* filePath) {
    const String& path = Checked(filePath, "WebCore::SharedBuffer::createWithContentsOfFile");
    bool mappingSuccess = false;
    WTF::MappedFileData mappedFileData{};
    _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(&mappedFileData, path, SharedMapping, mappingSuccess);
    if (!mappingSuccess) {
        _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(&mappedFileData);
        return CreateFromReadingFile(path);
    }
    auto* buffer = static_cast<SharedBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SharedBuffer)));
    ConstructFromMappedFile(*buffer, mappedFileData);
    _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(&mappedFileData);
    return SharedBufferRef(buffer);
}

SharedBufferRef APS5_VABI _ZN7WebCore12SharedBuffer21createFromReadingFileERKN3WTF6StringE(const String* filePath) {
    return CreateFromReadingFile(Checked(filePath, "WebCore::SharedBuffer::createFromReadingFile"));
}

SharedBufferDataViewResult APS5_VABI _ZNK7WebCore12SharedBuffer11getSomeDataEm(const SharedBuffer* self, std::size_t position) {
    const auto& buffer = Checked(self, "WebCore::SharedBuffer::getSomeData");
    if (position >= buffer.size) throw std::out_of_range("WebCore::SharedBuffer::getSomeData position is past the end");
    const DataSegmentVectorEntry* begin = buffer.segments.buffer;
    const DataSegmentVectorEntry* end = begin + buffer.segments.size;
    const DataSegmentVectorEntry* element = std::upper_bound(begin, end, position, [](std::size_t value, const DataSegmentVectorEntry& entry) {
        return value < entry.beginPosition;
    });
    --element;
    RefSegment(*element->segment);
    return SharedBufferDataViewResult({position - element->beginPosition, element->segment});
}

String APS5_VABI _ZNK7WebCore12SharedBuffer11toHexStringEv(const SharedBuffer* self) {
    const auto& buffer = Checked(self, "WebCore::SharedBuffer::toHexString");
    if (!buffer.size) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    if (buffer.size > std::numeric_limits<unsigned>::max() / 2)
        throw std::length_error("WebCore::SharedBuffer::toHexString result exceeds the WTF::String length limit");
    const auto* bytes = reinterpret_cast<const unsigned char*>(_ZNK7WebCore12SharedBuffer4dataEv(&buffer));
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(buffer.size * 2), true, characters));
    auto* output = static_cast<unsigned char*>(characters);
    static const char digits[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < buffer.size; ++i) {
        output[2 * i] = static_cast<unsigned char>(digits[bytes[i] >> 4]);
        output[2 * i + 1] = static_cast<unsigned char>(digits[bytes[i] & 0xF]);
    }
    return result;
}

void APS5_VABI _ZNK7WebCore12SharedBuffer23hintMemoryNotNeededSoonEv(const SharedBuffer* self) {
    Checked(self, "WebCore::SharedBuffer::hintMemoryNotNeededSoon");
}

bool APS5_VABI _ZNK7WebCore12SharedBuffereqERKS0_(const SharedBuffer* self, const SharedBuffer* other) {
    return Equal(Checked(self, "WebCore::SharedBuffer::operator=="), Checked(other, "WebCore::SharedBuffer::operator=="));
}

bool APS5_VABI _ZNK7WebCore12SharedBufferneERKS0_(const SharedBuffer* self, const SharedBuffer* other) {
    return !Equal(Checked(self, "WebCore::SharedBuffer::operator!="), Checked(other, "WebCore::SharedBuffer::operator!="));
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewC1EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(SharedBufferDataView* self, DataSegment** segment, std::size_t positionWithinSegment) {
    ConstructView(self, segment, positionWithinSegment);
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewC2EON3WTF3RefINS_12SharedBuffer11DataSegmentENS1_13DumbPtrTraitsIS4_EEEEm(SharedBufferDataView* self, DataSegment** segment, std::size_t positionWithinSegment) {
    ConstructView(self, segment, positionWithinSegment);
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewC1ERKS0_(SharedBufferDataView* self, const SharedBufferDataView* other) {
    CopyView(self, other);
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewC2ERKS0_(SharedBufferDataView* self, const SharedBufferDataView* other) {
    CopyView(self, other);
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewD1Ev(SharedBufferDataView* self) {
    DestroyView(self);
}

void APS5_VABI _ZN7WebCore20SharedBufferDataViewD2Ev(SharedBufferDataView* self) {
    DestroyView(self);
}

SharedBufferDataView* APS5_VABI _ZN7WebCore20SharedBufferDataViewaSERKS0_(SharedBufferDataView* self, const SharedBufferDataView* other) {
    const auto& source = Checked(other, "WebCore::SharedBufferDataView::operator=");
    auto& view = Checked(self, "WebCore::SharedBufferDataView::operator=");
    RefSegment(Checked(source.segment, "WebCore::SharedBufferDataView::operator="));
    DataSegment* old = std::exchange(view.segment, source.segment);
    view.positionWithinSegment = source.positionWithinSegment;
    DerefSegment(old);
    return &view;
}

const char* APS5_VABI _ZNK7WebCore20SharedBufferDataView4dataEv(const SharedBufferDataView* self) {
    const auto& view = Checked(self, "WebCore::SharedBufferDataView::data");
    return SegmentData(Checked(view.segment, "WebCore::SharedBufferDataView::data")) + view.positionWithinSegment;
}

std::size_t APS5_VABI _ZNK7WebCore20SharedBufferDataView4sizeEv(const SharedBufferDataView* self) {
    const auto& view = Checked(self, "WebCore::SharedBufferDataView::size");
    return SegmentSize(Checked(view.segment, "WebCore::SharedBufferDataView::size")) - view.positionWithinSegment;
}

}
