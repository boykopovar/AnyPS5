// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/FormData.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/FormData.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/BlobData.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/Vector.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/FormDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::FormDataByteVector;
using NKWebKit::WebCoreAbi::FormDataElement;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

constexpr std::int64_t kToEndOfFile = -1;
constexpr std::size_t kMinCapacity = 16;
constexpr std::uint32_t kBytes = 0;
constexpr std::uint32_t kFile = 1;
constexpr std::uint32_t kBlob = 2;
constexpr std::uint8_t kValueless = 0xFF;

FormData& Checked(FormData* self) {
    if (!self) throw std::invalid_argument("null WebCore::FormData");
    return *self;
}

const FormData& Checked(const FormData* self) {
    if (!self) throw std::invalid_argument("null WebCore::FormData");
    return *self;
}

const URL& Checked(const URL* url) {
    if (!url) throw std::invalid_argument("null WTF::URL");
    return *url;
}

template<class TVector>
void Validate(const TVector& vector) {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid WebCore::FormData vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null WebCore::FormData vector storage");
}

std::uint32_t Kind(const FormDataElement& element) {
    const auto index = static_cast<std::uint8_t>(element.index & 0xFF);
    if (index == kValueless) throw std::logic_error("valueless WebCore::FormDataElement");
    if (index > kBlob) throw std::logic_error("invalid WebCore::FormDataElement index");
    return index;
}

template<class TVector>
void Grow(TVector& vector, std::uint64_t minimum) {
    using TElement = std::remove_pointer_t<decltype(vector.buffer)>;
    constexpr std::uint64_t limit = std::numeric_limits<unsigned>::max() / sizeof(TElement);
    const std::uint64_t grown = std::uint64_t(vector.capacity) + vector.capacity / 4 + 1;
    const std::uint64_t capacity = std::max({minimum, std::uint64_t(kMinCapacity), grown});
    if (capacity > limit) throw std::length_error("WebCore::FormData vector capacity overflow");
    auto* buffer = static_cast<TElement*>(_ZN3WTF10fastMallocEm(capacity * sizeof(TElement)));
    if (vector.size) std::memcpy(buffer, vector.buffer, std::size_t(vector.size) * sizeof(TElement));
    _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = buffer;
    vector.capacity = static_cast<unsigned>(capacity);
}

template<class TVector>
void AppendBytes(TVector& vector, const void* data, std::size_t size) {
    if (!size) return;
    Validate(vector);
    const std::uint64_t total = std::uint64_t(vector.size) + size;
    if (total > std::numeric_limits<unsigned>::max()) throw std::length_error("WebCore::FormData bytes size overflow");
    if (total > vector.capacity) Grow(vector, total);
    std::memcpy(vector.buffer + vector.size, data, size);
    vector.size = static_cast<unsigned>(total);
}

FormDataByteVector ExactBytes(const void* data, std::size_t size) {
    if (size > std::numeric_limits<unsigned>::max()) throw std::length_error("WebCore::FormData bytes size overflow");
    if (!size) return {};
    auto* buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(size));
    std::memcpy(buffer, data, size);
    return {buffer, static_cast<unsigned>(size), static_cast<unsigned>(size)};
}

FormDataElement& NextSlot(FormData& formData) {
    Validate(formData.elements);
    if (formData.elements.size == formData.elements.capacity) Grow(formData.elements, std::uint64_t(formData.elements.size) + 1);
    return formData.elements.buffer[formData.elements.size];
}

void Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
}

struct Borrowed {
    NKWebKit::WtfAbi::String value;
    explicit Borrowed(StringImpl* impl) : value(impl) {}
    Borrowed(const Borrowed&) = delete;
    Borrowed& operator=(const Borrowed&) = delete;
    ~Borrowed() { value.impl = nullptr; }
};

StringImpl* IsolatedCopy(StringImpl* impl) {
    Borrowed borrowed(impl);
    auto copy = _ZNKR3WTF6String12isolatedCopyEv(&borrowed.value);
    return std::exchange(copy.impl, nullptr);
}

void AppendData(FormData& formData, const void* data, std::size_t size) {
    formData.hasLengthInBytes = false;
    Validate(formData.elements);
    if (formData.elements.size) {
        auto& last = formData.elements.buffer[formData.elements.size - 1];
        if (Kind(last) == kBytes) return AppendBytes(last.storage.bytes, data, size);
    }
    auto& slot = NextSlot(formData);
    slot.storage.bytes = ExactBytes(data, size);
    slot.index = kBytes;
    ++formData.elements.size;
}

void Destroy(FormData& formData) {
    _ZN3WTF8fastFreeEPv(formData.boundary.buffer);
    formData.boundary = {};
    for (unsigned i = 0; i < formData.elements.size; ++i) {
        auto& element = formData.elements.buffer[i];
        if (static_cast<std::uint8_t>(element.index & 0xFF) == kValueless) continue;
        switch (element.index & 0xFF) {
        case kBytes:
            _ZN3WTF8fastFreeEPv(element.storage.bytes.buffer);
            break;
        case kFile:
            NKWebKit::WtfAbi::Release(element.storage.file.filename);
            break;
        case kBlob:
            NKWebKit::WtfAbi::Release(element.storage.blob.url.impl);
            break;
        }
    }
    _ZN3WTF8fastFreeEPv(formData.elements.buffer);
    formData.elements = {};
}

FormData* NewFormData() {
    void* storage = _ZN3WTF10fastMallocEm(sizeof(FormData));
    auto* formData = new (storage) FormData{};
    formData->refCount = 1;
    return formData;
}

void Discard(FormData* formData) {
    Destroy(*formData);
    _ZN3WTF8fastFreeEPv(formData);
}

WebCore::Ref<FormData> Adopt(FormData* formData) {
    return WebCore::Ref<FormData>(*formData);
}

void CopyElement(FormDataElement& destination, const FormDataElement& source) {
    switch (Kind(source)) {
    case kBytes:
        destination.storage.bytes = ExactBytes(source.storage.bytes.buffer, source.storage.bytes.size);
        break;
    case kFile: {
        auto& file = destination.storage.file;
        file = source.storage.file;
        file.filename = IsolatedCopy(source.storage.file.filename);
        break;
    }
    case kBlob: {
        auto& blob = destination.storage.blob;
        blob = source.storage.blob;
        blob.url.impl = IsolatedCopy(source.storage.blob.url.impl);
        break;
    }
    }
    destination.index = source.index & 0xFF;
}

}

namespace NKWebKit::WebCoreAbi {

void FormData::deref() const {
    if (refCount != 1) {
        --refCount;
        return;
    }
    auto* self = const_cast<FormData*>(this);
    _ZN7WebCore8FormDataD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

}

extern "C" {

WebCore::Ref<FormData> APS5_VABI _ZN7WebCore8FormData6createEv() {
    return Adopt(NewFormData());
}

WebCore::Ref<FormData> APS5_VABI _ZN7WebCore8FormData6createEPKvm(const void* data, std::size_t size) {
    if (!data && size) throw std::invalid_argument("null FormData bytes");
    auto* formData = NewFormData();
    try {
        AppendData(*formData, data, size);
    } catch (...) {
        Discard(formData);
        throw;
    }
    return Adopt(formData);
}

void APS5_VABI _ZN7WebCore8FormData10appendDataEPKvm(FormData* self, const void* data, std::size_t size) {
    auto& formData = Checked(self);
    if (!data && size) throw std::invalid_argument("null FormData bytes");
    AppendData(formData, data, size);
}

void APS5_VABI _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(FormData* self, const URL* blobURL) {
    auto& formData = Checked(self);
    const URL url = Checked(blobURL);
    auto& slot = NextSlot(formData);
    Retain(url.impl);
    slot.storage.blob.url = url;
    slot.index = kBlob;
    ++formData.elements.size;
    formData.hasLengthInBytes = false;
}

void APS5_VABI _ZN7WebCore8FormDataD1Ev(FormData* self) {
    auto& formData = Checked(self);
    Validate(formData.elements);
    Validate(formData.boundary);
    for (unsigned i = 0; i < formData.elements.size; ++i) {
        const auto& element = formData.elements.buffer[i];
        if (static_cast<std::uint8_t>(element.index & 0xFF) == kValueless) continue;
        if (Kind(element) == kBytes) Validate(element.storage.bytes);
    }
    Destroy(formData);
}

void APS5_VABI _ZN7WebCore8FormDataD2Ev(FormData* self) {
    _ZN7WebCore8FormDataD1Ev(self);
}

WebCore::Ref<FormData> APS5_VABI _ZNK7WebCore8FormData12isolatedCopyEv(const FormData* self) {
    const auto& source = Checked(self);
    Validate(source.elements);
    auto* copy = NewFormData();
    try {
        copy->alwaysStream = source.alwaysStream;
        if (source.elements.size) {
            copy->elements.buffer = static_cast<FormDataElement*>(_ZN3WTF10fastMallocEm(std::size_t(source.elements.size) * sizeof(FormDataElement)));
            copy->elements.capacity = source.elements.size;
        }
        for (unsigned i = 0; i < source.elements.size; ++i) {
            CopyElement(copy->elements.buffer[i], source.elements.buffer[i]);
            ++copy->elements.size;
        }
    } catch (...) {
        Discard(copy);
        throw;
    }
    return Adopt(copy);
}

NKWebKit::WtfAbi::Vector<std::uint8_t> APS5_VABI _ZNK7WebCore8FormData7flattenEv(const FormData* self) {
    const auto& formData = Checked(self);
    Validate(formData.elements);
    NKWebKit::WtfAbi::Vector<std::uint8_t> result;
    for (unsigned i = 0; i < formData.elements.size; ++i) {
        const auto& element = formData.elements.buffer[i];
        if (Kind(element) != kBytes) continue;
        Validate(element.storage.bytes);
        AppendBytes(result, element.storage.bytes.buffer, element.storage.bytes.size);
    }
    return result;
}

URL APS5_VABI _ZNK7WebCore8FormData9asBlobURLEv(const FormData* self) {
    const auto& formData = Checked(self);
    Validate(formData.elements);
    if (formData.elements.size != 1) return {};
    const auto& element = formData.elements.buffer[0];
    if (Kind(element) != kBlob) return {};
    Retain(element.storage.blob.url.impl);
    return element.storage.blob.url;
}

std::uint64_t APS5_VABI _ZNK7WebCore8FormData13lengthInBytesEv(const FormData* self) {
    const auto& formData = Checked(self);
    if (formData.hasLengthInBytes) return formData.lengthInBytes;
    Validate(formData.elements);
    std::uint64_t length = 0;
    for (unsigned i = 0; i < formData.elements.size; ++i) {
        const auto& element = formData.elements.buffer[i];
        switch (Kind(element)) {
        case kBytes:
            length += element.storage.bytes.size;
            break;
        case kFile:
            if (element.storage.file.fileLength == kToEndOfFile) throw std::logic_error("WebCore::FormData file range to the end of the file needs FileSystem::fileSize");
            length += static_cast<std::uint64_t>(element.storage.file.fileLength);
            break;
        case kBlob:
            throw std::logic_error("WebCore::FormData blob length needs ThreadableBlobRegistry::blobSize");
        }
    }
    formData.lengthInBytes = length;
    formData.hasLengthInBytes = true;
    return length;
}

}
