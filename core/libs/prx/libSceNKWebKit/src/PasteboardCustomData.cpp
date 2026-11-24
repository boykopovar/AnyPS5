// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/PasteboardCustomData.cpp
/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
 */

// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/persistence/PersistentDecoder.cpp
/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
 */

// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/persistence/PersistentCoders.cpp
/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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
 */

#include "prx/libSceNKWebKit/include/PasteboardCustomDataExports.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"
#include "prx/libSceNKWebKit/src/WtfHashMapStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfPersistenceChecksum.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

using NKWebKit::WebCoreAbi::PasteboardCustomData;
using NKWebKit::WebCoreAbi::PasteboardEntry;
using NKWebKit::WebCoreAbi::PasteboardPlatformData;
using NKWebKit::WebCoreAbi::SharedBuffer;
using NKWebKit::WebCoreAbi::SharedBufferRef;
using namespace NKWebKit::WtfAbi;

namespace NKWebKit::WebCoreAbi {

PasteboardPlatformData::PasteboardPlatformData() : string(nullptr), index(0) {}

PasteboardPlatformData::PasteboardPlatformData(const PasteboardPlatformData& other) : PasteboardPlatformData() {
    if (other.index == 0) {
        string = other.string;
        if (string) string->refCount += 2;
    } else if (other.index == 1) {
        index = 1;
        buffer = other.buffer;
        if (buffer) ++buffer->refCount;
    } else if (other.index != -1) {
        throw std::invalid_argument("PasteboardCustomData: invalid platform data variant");
    }
}

PasteboardPlatformData::PasteboardPlatformData(PasteboardPlatformData&& other) noexcept : index(other.index) {
    if (index == 1) buffer = std::exchange(other.buffer, nullptr);
    else string = std::exchange(other.string, nullptr);
}

PasteboardPlatformData::~PasteboardPlatformData() {
    if (index == 0) Release(string);
    else if (index == 1) { SharedBufferRef reference(buffer); }
}

PasteboardPlatformData& PasteboardPlatformData::operator=(PasteboardPlatformData other) noexcept {
    PasteboardPlatformData previous(std::move(*this));
    index = other.index;
    if (index == 1) buffer = std::exchange(other.buffer, nullptr);
    else string = std::exchange(other.string, nullptr);
    return *this;
}

}  // namespace NKWebKit::WebCoreAbi

namespace {

template <typename T>
T& Checked(T* pointer) {
    if (!pointer) throw std::invalid_argument("PasteboardCustomData: null argument");
    return *pointer;
}

struct StringMap {
    StringHashMap value{nullptr};
    ~StringMap() { StringHashMapClear(value); }
};

struct PersistenceEncoder {
    Encoder value;
    PersistenceEncoder() { _ZN3WTF11Persistence7EncoderC1Ev(&value); }
    ~PersistenceEncoder() { _ZN3WTF11Persistence7EncoderD1Ev(&value); }
};

void EncodeString(Encoder& encoder, StringImpl* impl) {
    String string;
    string.impl = impl;
    if (impl) impl->refCount += 2;
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(encoder, string);
}

SharedBufferRef Encode(const PasteboardCustomData& data) {
    StringMap map;
    for (unsigned i = 0; i < data.data.size; ++i) {
        const auto& entry = data.data[i];
        StringHashMapSet(map.value, entry.type.impl, entry.customData.impl);
    }
    PersistenceEncoder encoder;
    _ZN3WTF11Persistence7EncoderlsEj(&encoder.value, 1);
    EncodeString(encoder.value, data.origin.impl);
    _ZN3WTF11Persistence7EncoderlsEm(&encoder.value, StringHashMapKeyCount(map.value));
    for (auto iterator = StringHashMapBegin(map.value); iterator.position != iterator.endPosition; StringHashMapAdvance(iterator)) {
        EncodeString(encoder.value, iterator.position->key);
        EncodeString(encoder.value, iterator.position->value);
    }
    _ZN3WTF11Persistence7EncoderlsEm(&encoder.value, data.data.size);
    for (unsigned i = 0; i < data.data.size; ++i) EncodeString(encoder.value, data.data[i].type.impl);
    return _ZN7WebCore12SharedBuffer6createEPKcm(reinterpret_cast<const char*>(encoder.value.buffer), encoder.value.size);
}

bool CanRead(const Decoder& decoder, std::size_t size) {
    const auto start = reinterpret_cast<std::uintptr_t>(decoder.bufferData);
    const auto position = reinterpret_cast<std::uintptr_t>(decoder.bufferPosition);
    if (position < start || position - start > decoder.bufferSize) return false;
    return size <= decoder.bufferSize - (position - start);
}

bool Read(Decoder& decoder, void* destination, std::size_t size, unsigned salt) {
    if (!CanRead(decoder, size)) return false;
    if (size && !GuestCopyIn_nid_no_patch(destination, decoder.bufferPosition, size))
        throw std::invalid_argument("PasteboardCustomData: unreadable persistence data");
    if (size) decoder.bufferPosition += size;
    PersistenceChecksumAdd_nid_no_patch(decoder.sha1, &salt, sizeof(salt));
    PersistenceChecksumAdd_nid_no_patch(decoder.sha1, destination, size);
    return true;
}

bool DecodeString(Decoder& decoder, String& result) {
    std::uint32_t length;
    if (!Read(decoder, &length, sizeof(length), 11)) return false;
    if (length == std::numeric_limits<std::uint32_t>::max()) {
        result = String();
        return true;
    }
    std::uint8_t is8Bit;
    if (!Read(decoder, &is8Bit, sizeof(is8Bit), 3)) return false;
    if (is8Bit > 1) throw std::invalid_argument("PasteboardCustomData: invalid persistence boolean");
    const std::size_t byteLength = std::size_t(length) * (is8Bit ? 1 : 2);
    if (!CanRead(decoder, byteLength)) return false;
    void* characters;
    String value(CreateUninitialized(length, is8Bit, characters));
    if (!Read(decoder, characters, byteLength, 101)) return false;
    result = std::move(value);
    return true;
}

PasteboardCustomData Decode(Decoder& decoder) {
    std::uint32_t version;
    if (!Read(decoder, &version, sizeof(version), 11) || version > 1) return {};
    PasteboardCustomData result;
    if (!DecodeString(decoder, result.origin)) return {};
    std::uint64_t count;
    if (!Read(decoder, &count, sizeof(count), 13)) return {};
    StringMap map;
    for (std::uint64_t i = 0; i < count; ++i) {
        String key;
        String value;
        if (!DecodeString(decoder, key) || !DecodeString(decoder, value)) return {};
        if (!StringHashMapAdd(map.value, key.impl, value.impl).isNewEntry) return {};
    }
    if (!Read(decoder, &count, sizeof(count), 13)) return {};
    Vector<String> types;
    for (std::uint64_t i = 0; i < count; ++i) {
        String type;
        if (!DecodeString(decoder, type)) return {};
        if (types.size == types.capacity && types.capacity > std::numeric_limits<unsigned>::max() / 2)
            throw std::length_error("PasteboardCustomData: too many types");
        types.append(type);
    }
    for (unsigned i = 0; i < types.size; ++i) {
        const String& type = types[i];
        PasteboardEntry entry;
        entry.type = type;
        if (auto* pair = StringHashMapFind(map.value, type.impl)) {
            entry.customData.impl = pair->value;
            if (entry.customData.impl) entry.customData.impl->refCount += 2;
        }
        for (unsigned j = 0; j < result.data.size; ++j) {
            if (!_ZN3WTF5equalEPKNS_10StringImplES2_(result.data[j].type.impl, type.impl)) continue;
            for (unsigned k = j; k + 1 < result.data.size; ++k) result.data[k] = std::move(result.data[k + 1]);
            result.data[--result.data.size].~PasteboardEntry();
            break;
        }
        result.data.append(entry);
    }
    return result;
}

}  // namespace

extern "C" {

PasteboardCustomData APS5_VABI _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(const SharedBuffer* buffer) {

    const auto& source = Checked(buffer);
    Decoder decoder{};
    decoder.bufferData = reinterpret_cast<const std::uint8_t*>(_ZNK7WebCore12SharedBuffer4dataEv(&source));
    decoder.bufferSize = _ZNK7WebCore12SharedBuffer4sizeEv(&source);
    decoder.bufferPosition = decoder.bufferData;
    _ZN3WTF4SHA1C1Ev(&decoder.sha1);
    return Decode(decoder);
}

PasteboardCustomData APS5_VABI _ZN7WebCore20PasteboardCustomData22fromPersistenceDecoderEON3WTF11Persistence7DecoderE(Decoder* decoder) {
    return Decode(Checked(decoder));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1EOS1_(PasteboardEntry* self, PasteboardEntry* other) {
    new (&Checked(self)) PasteboardEntry(std::move(Checked(other)));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1ERKS1_(PasteboardEntry* self, const PasteboardEntry* other) {
    new (&Checked(self)) PasteboardEntry(Checked(other));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1Ev(PasteboardEntry* self) {
    new (&Checked(self)) PasteboardEntry{};
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2EOS1_(PasteboardEntry* self, PasteboardEntry* other) {
    new (&Checked(self)) PasteboardEntry(std::move(Checked(other)));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2ERKS1_(PasteboardEntry* self, const PasteboardEntry* other) {
    new (&Checked(self)) PasteboardEntry(Checked(other));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2Ev(PasteboardEntry* self) {
    new (&Checked(self)) PasteboardEntry{};
}

PasteboardEntry& APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryaSEOS1_(PasteboardEntry* self, PasteboardEntry* other) {
    return Checked(self) = std::move(Checked(other));
}

PasteboardEntry& APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryaSERKS1_(PasteboardEntry* self, const PasteboardEntry* other) {
    return Checked(self) = Checked(other);
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::PasteboardCustomData* self, String* origin, Vector<PasteboardEntry>* data) {
    new (&Checked(self)) PasteboardCustomData{std::move(Checked(origin)), std::move(Checked(data))};
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EOS0_(PasteboardCustomData* self, PasteboardCustomData* other) {
    new (&Checked(self)) PasteboardCustomData(std::move(Checked(other)));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1ERKS0_(PasteboardCustomData* self, const PasteboardCustomData* other) {
    new (&Checked(self)) PasteboardCustomData(Checked(other));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1Ev(PasteboardCustomData* self) {
    new (&Checked(self)) PasteboardCustomData{};
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::PasteboardCustomData* self, String* origin, Vector<PasteboardEntry>* data) {
    new (&Checked(self)) PasteboardCustomData{std::move(Checked(origin)), std::move(Checked(data))};
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EOS0_(PasteboardCustomData* self, PasteboardCustomData* other) {
    new (&Checked(self)) PasteboardCustomData(std::move(Checked(other)));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2ERKS0_(PasteboardCustomData* self, const PasteboardCustomData* other) {
    new (&Checked(self)) PasteboardCustomData(Checked(other));
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2Ev(PasteboardCustomData* self) {
    new (&Checked(self)) PasteboardCustomData{};
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataD1Ev(PasteboardCustomData* self) {
    Checked(self).~PasteboardCustomData();
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataD2Ev(PasteboardCustomData* self) {
    Checked(self).~PasteboardCustomData();
}

PasteboardCustomData& APS5_VABI _ZN7WebCore20PasteboardCustomDataaSERKS0_(PasteboardCustomData* self, const PasteboardCustomData* other) {
    return Checked(self) = Checked(other);
}

SharedBufferRef APS5_VABI _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(const PasteboardCustomData* self) {
    return Encode(Checked(self));
}

}
