// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOSchemeInformationBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOProtectionSchemeInfoBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOProtectionSystemSpecificHeaderBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOSchemeInformationBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOProtectionSchemeInfoBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOProtectionSystemSpecificHeaderBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/IsoSchemeBoxExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::IsoAbi::Box;
using NKWebKit::IsoAbi::ByteVector;
using NKWebKit::IsoAbi::OptionalByteVector;
using NKWebKit::IsoAbi::ProtectionSchemeInfoBox;
using NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox;
using NKWebKit::IsoAbi::SchemeInformationBox;
using NKWebKit::IsoAbi::SchemeTypeBox;
using NKWebKit::IsoAbi::TrackEncryptionBox;
using KeyIDVector = NKWebKit::WtfAbi::Vector<ByteVector>;
using namespace NKWebKit::IsoParse;

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

void APS5_VABI SchemeInformationCompleteDestructor(void* self) {
    _ZN7WebCore23ISOSchemeInformationBoxD1Ev(static_cast<SchemeInformationBox*>(self));
}

void APS5_VABI SchemeInformationDeletingDestructor(void* self) {
    _ZN7WebCore23ISOSchemeInformationBoxD0Ev(static_cast<SchemeInformationBox*>(self));
}

bool APS5_VABI SchemeInformationParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore23ISOSchemeInformationBox5parseERN3JSC8DataViewERj(static_cast<SchemeInformationBox*>(self), view, offset);
}

void APS5_VABI SchemeInfoCompleteDestructor(void* self) {
    _ZN7WebCore26ISOProtectionSchemeInfoBoxD2Ev(static_cast<ProtectionSchemeInfoBox*>(self));
}

void APS5_VABI SchemeInfoDeletingDestructor(void* self) {
    _ZN7WebCore26ISOProtectionSchemeInfoBoxD0Ev(static_cast<ProtectionSchemeInfoBox*>(self));
}

bool APS5_VABI SchemeInfoParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore26ISOProtectionSchemeInfoBox5parseERN3JSC8DataViewERj(static_cast<ProtectionSchemeInfoBox*>(self), view, offset);
}

void APS5_VABI HeaderCompleteDestructor(void* self) {
    _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD1Ev(static_cast<ProtectionSystemSpecificHeaderBox*>(self));
}

void APS5_VABI HeaderDeletingDestructor(void* self) {
    _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD0Ev(static_cast<ProtectionSystemSpecificHeaderBox*>(self));
}

bool APS5_VABI HeaderParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore36ISOProtectionSystemSpecificHeaderBox5parseERN3JSC8DataViewERj(static_cast<ProtectionSystemSpecificHeaderBox*>(self), view, offset);
}

const Vtable SchemeInformationTable{SchemeInformationCompleteDestructor, SchemeInformationDeletingDestructor, SchemeInformationParse};
const Vtable SchemeInfoTable{SchemeInfoCompleteDestructor, SchemeInfoDeletingDestructor, SchemeInfoParse};
const Vtable HeaderTable{HeaderCompleteDestructor, HeaderDeletingDestructor, HeaderParse};

template<typename T> void ResetOwned(T*& slot, T* replacement) {
    T* old = slot;
    slot = replacement;
    DeleteBox(old ? reinterpret_cast<Box*>(old) : nullptr);
}

void DestroyKeyIDs(KeyIDVector& keyIDs) {
    for (unsigned i = 0; i < keyIDs.size; ++i)
        Destroy(keyIDs.buffer[i]);
    if (keyIDs.buffer)
        _ZN3WTF8fastFreeEPv(keyIDs.buffer);
    keyIDs.buffer = nullptr;
    keyIDs.capacity = 0;
    keyIDs.size = 0;
}

bool TryReserveKeyIDs(KeyIDVector& keyIDs, unsigned capacity) {
    if (capacity <= keyIDs.capacity)
        return true;
    if (capacity > std::numeric_limits<unsigned>::max() / sizeof(ByteVector))
        return false;
    auto* buffer = static_cast<ByteVector*>(_ZN3WTF10fastMallocEm(std::size_t(capacity) * sizeof(ByteVector)));
    if (keyIDs.size)
        std::memcpy(static_cast<void*>(buffer), keyIDs.buffer, std::size_t(keyIDs.size) * sizeof(ByteVector));
    if (keyIDs.buffer)
        _ZN3WTF8fastFreeEPv(keyIDs.buffer);
    keyIDs.buffer = buffer;
    keyIDs.capacity = capacity;
    return true;
}

void ResizeKeyIDs(KeyIDVector& keyIDs, unsigned size) {
    if (size < keyIDs.size) {
        for (unsigned i = size; i < keyIDs.size; ++i)
            Destroy(keyIDs.buffer[i]);
    } else if (size > keyIDs.size) {
        if (size > keyIDs.capacity) {
            std::size_t grown = std::max<std::size_t>(16, std::size_t(keyIDs.capacity) + keyIDs.capacity / 4 + 1);
            TryReserveKeyIDs(keyIDs, static_cast<unsigned>(std::max<std::size_t>(size, grown)));
        }
        std::memset(static_cast<void*>(keyIDs.buffer + keyIDs.size), 0, std::size_t(size - keyIDs.size) * sizeof(ByteVector));
    }
    keyIDs.size = size;
}

void CopyKeyIDs(KeyIDVector& target, const KeyIDVector& source) {
    target.buffer = nullptr;
    target.capacity = 0;
    target.size = 0;
    if (!source.size)
        return;
    target.buffer = static_cast<ByteVector*>(_ZN3WTF10fastMallocEm(std::size_t(source.size) * sizeof(ByteVector)));
    target.capacity = source.size;
    for (unsigned i = 0; i < source.size; ++i)
        new (&target.buffer[i]) ByteVector(source.buffer[i]);
    target.size = source.size;
}

bool CopySlice(ByteVector& target, const void* buffer, unsigned& offset, std::uint64_t length) {
    auto slice = Slice(buffer, offset, static_cast<unsigned>(offset + length));
    offset += static_cast<unsigned>(length);
    Resize(target, static_cast<unsigned>(length));
    if (slice.size() < length)
        return false;
    if (length)
        std::memcpy(target.buffer, slice.data(), static_cast<std::size_t>(length));
    return true;
}

void ConstructHeader(ProtectionSystemSpecificHeaderBox& box) {
    ConstructFullBox(box.fullBox, &HeaderTable);
    new (&box.systemID) ByteVector();
    box.keyIDs.buffer = nullptr;
    box.keyIDs.capacity = 0;
    box.keyIDs.size = 0;
    new (&box.data) ByteVector();
}

void DestroyHeader(ProtectionSystemSpecificHeaderBox& box) {
    box.fullBox.box.vtable = reinterpret_cast<const void* const*>(&HeaderTable);
    Destroy(box.data);
    DestroyKeyIDs(box.keyIDs);
    Destroy(box.systemID);
    DestroyBox(box.fullBox.box, BoxVtable());
}

bool ParseHeader(ProtectionSystemSpecificHeaderBox& box, const DataView& view, unsigned& offset) {
    if (!ParseFullBox(box.fullBox, view, offset))
        return false;

    const void* buffer = ViewBuffer(view);
    if (!buffer)
        return false;

    if (!CopySlice(box.systemID, buffer, offset, 16))
        return false;

    if (box.fullBox.version) {
        std::uint32_t keyIDCount = 0;
        if (!ReadU32(view, offset, keyIDCount))
            return false;
        if (BufferByteLength(buffer) - offset < static_cast<std::uint32_t>(keyIDCount * 16))
            return false;
        if (!TryReserveKeyIDs(box.keyIDs, keyIDCount))
            return false;
        ResizeKeyIDs(box.keyIDs, keyIDCount);
        for (unsigned keyID = 0; keyID < keyIDCount; keyID++) {
            auto& currentKeyID = box.keyIDs.buffer[keyID];
            Resize(currentKeyID, 16);
            auto parsedKeyID = Slice(buffer, offset, static_cast<unsigned>(offset + 16));
            offset += 16;
            if (parsedKeyID.size() < 16)
                continue;
            std::memcpy(currentKeyID.buffer, parsedKeyID.data(), 16);
        }
    }

    std::uint32_t dataSize = 0;
    if (!ReadU32(view, offset, dataSize))
        return false;
    if (BufferByteLength(buffer) - offset < dataSize)
        return false;
    return CopySlice(box.data, buffer, offset, dataSize);
}

void ConstructSchemeInfo(ProtectionSchemeInfoBox& box) {
    ConstructBox(box.box, &SchemeInfoTable);
    _ZN7WebCore20ISOOriginalFormatBoxC2Ev(&box.originalFormatBox);
    box.schemeTypeBox = nullptr;
    box.schemeInformationBox = nullptr;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxC1Ev(SchemeInformationBox* self) {
    auto& box = Checked(self, "WebCore::ISOSchemeInformationBox::ISOSchemeInformationBox");
    ConstructBox(box.box, &SchemeInformationTable);
    box.schemeSpecificData = nullptr;
}

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxC2Ev(SchemeInformationBox* self) {
    _ZN7WebCore23ISOSchemeInformationBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD1Ev(SchemeInformationBox* self) {
    auto& box = Checked(self, "WebCore::ISOSchemeInformationBox::~ISOSchemeInformationBox");
    box.box.vtable = reinterpret_cast<const void* const*>(&SchemeInformationTable);
    ResetOwned(box.schemeSpecificData, static_cast<Box*>(nullptr));
    DestroyBox(box.box, BoxVtable());
}

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD2Ev(SchemeInformationBox* self) {
    _ZN7WebCore23ISOSchemeInformationBoxD1Ev(self);
}

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD0Ev(SchemeInformationBox* self) {
    _ZN7WebCore23ISOSchemeInformationBoxD1Ev(self);
    FreeBox(self);
}

bool APS5_VABI _ZN7WebCore23ISOSchemeInformationBox5parseERN3JSC8DataViewERj(SchemeInformationBox* self, DataView* view, unsigned* offset) {
    auto& box = Checked(self, "WebCore::ISOSchemeInformationBox::parse");
    const auto& data = CheckedView(view, "WebCore::ISOSchemeInformationBox::parse");
    unsigned& start = Checked(offset, "WebCore::ISOSchemeInformationBox::parse");
    unsigned localOffset = start;
    if (!ParseBox(box.box, data, localOffset))
        return false;

    auto schemeSpecificBoxType = PeekBox(data, localOffset);
    if (!schemeSpecificBoxType.engaged)
        return false;

    if (schemeSpecificBoxType.type == FourCC("tenc")) {
        if (std::uint64_t(localOffset) + schemeSpecificBoxType.size > std::uint64_t(start) + box.box.size)
            return false;

        auto* trackEncryption = static_cast<TrackEncryptionBox*>(AllocateBox(sizeof(TrackEncryptionBox)));
        _ZN7WebCore21ISOTrackEncryptionBoxC1Ev(trackEncryption);
        ResetOwned(box.schemeSpecificData, reinterpret_cast<Box*>(trackEncryption));
        if (!VirtualRead(*box.schemeSpecificData, data, localOffset))
            return false;
    }

    return true;
}

void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxC1Ev(ProtectionSchemeInfoBox* self) {
    ConstructSchemeInfo(Checked(self, "WebCore::ISOProtectionSchemeInfoBox::ISOProtectionSchemeInfoBox"));
}

void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxC2Ev(ProtectionSchemeInfoBox* self) {
    _ZN7WebCore26ISOProtectionSchemeInfoBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxD2Ev(ProtectionSchemeInfoBox* self) {
    auto& box = Checked(self, "WebCore::ISOProtectionSchemeInfoBox::~ISOProtectionSchemeInfoBox");
    box.box.vtable = reinterpret_cast<const void* const*>(&SchemeInfoTable);
    ResetOwned(box.schemeInformationBox, static_cast<SchemeInformationBox*>(nullptr));
    ResetOwned(box.schemeTypeBox, static_cast<SchemeTypeBox*>(nullptr));
    _ZN7WebCore20ISOOriginalFormatBoxD2Ev(&box.originalFormatBox);
    DestroyBox(box.box, BoxVtable());
}

void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxD0Ev(ProtectionSchemeInfoBox* self) {
    _ZN7WebCore26ISOProtectionSchemeInfoBoxD2Ev(self);
    FreeBox(self);
}

bool APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBox5parseERN3JSC8DataViewERj(ProtectionSchemeInfoBox* self, DataView* view, unsigned* offset) {
    auto& box = Checked(self, "WebCore::ISOProtectionSchemeInfoBox::parse");
    const auto& data = CheckedView(view, "WebCore::ISOProtectionSchemeInfoBox::parse");
    unsigned& start = Checked(offset, "WebCore::ISOProtectionSchemeInfoBox::parse");
    unsigned localOffset = start;
    if (!ParseBox(box.box, data, localOffset))
        return false;

    if (!VirtualRead(box.originalFormatBox.box, data, localOffset))
        return false;

    if (localOffset - start == box.box.size) {
        start = localOffset;
        return true;
    }

    auto optionalBoxType = PeekBox(data, localOffset);
    if (!optionalBoxType.engaged)
        return false;

    if (optionalBoxType.type == FourCC("schm")) {
        auto* schemeType = static_cast<SchemeTypeBox*>(AllocateBox(sizeof(SchemeTypeBox)));
        _ZN7WebCore16ISOSchemeTypeBoxC1Ev(schemeType);
        ResetOwned(box.schemeTypeBox, schemeType);
        if (!VirtualRead(box.schemeTypeBox->fullBox.box, data, localOffset))
            return false;

        if (localOffset - start == box.box.size) {
            start = localOffset;
            return true;
        }

        optionalBoxType = PeekBox(data, localOffset);
        if (!optionalBoxType.engaged)
            return false;
    }

    if (optionalBoxType.type == FourCC("schi")) {
        auto* schemeInformation = static_cast<SchemeInformationBox*>(AllocateBox(sizeof(SchemeInformationBox)));
        _ZN7WebCore23ISOSchemeInformationBoxC1Ev(schemeInformation);
        ResetOwned(box.schemeInformationBox, schemeInformation);
        if (!VirtualRead(box.schemeInformationBox->box, data, localOffset))
            return false;

        if (localOffset - start != box.box.size)
            return false;
    }

    start = localOffset;
    return true;
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1Ev(ProtectionSystemSpecificHeaderBox* self) {
    ConstructHeader(Checked(self, "WebCore::ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox"));
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC2Ev(ProtectionSystemSpecificHeaderBox* self) {
    _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC2ERKS0_(ProtectionSystemSpecificHeaderBox* self, const ProtectionSystemSpecificHeaderBox* other) {
    auto& box = Checked(self, "WebCore::ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox");
    const auto& source = Checked(other, "WebCore::ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox");
    CopyFullBox(box.fullBox, source.fullBox, &HeaderTable);
    new (&box.systemID) ByteVector(source.systemID);
    CopyKeyIDs(box.keyIDs, source.keyIDs);
    new (&box.data) ByteVector(source.data);
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD1Ev(ProtectionSystemSpecificHeaderBox* self) {
    DestroyHeader(Checked(self, "WebCore::ISOProtectionSystemSpecificHeaderBox::~ISOProtectionSystemSpecificHeaderBox"));
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD2Ev(ProtectionSystemSpecificHeaderBox* self) {
    _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD1Ev(self);
}

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD0Ev(ProtectionSystemSpecificHeaderBox* self) {
    _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD1Ev(self);
    FreeBox(self);
}

bool APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBox5parseERN3JSC8DataViewERj(ProtectionSystemSpecificHeaderBox* self, DataView* view, unsigned* offset) {
    return ParseHeader(Checked(self, "WebCore::ISOProtectionSystemSpecificHeaderBox::parse"), CheckedView(view, "WebCore::ISOProtectionSystemSpecificHeaderBox::parse"), Checked(offset, "WebCore::ISOProtectionSystemSpecificHeaderBox::parse"));
}

OptionalByteVector APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBox12peekSystemIDERN3JSC8DataViewEj(DataView* view, unsigned offset) {
    const auto& data = CheckedView(view, "WebCore::ISOProtectionSystemSpecificHeaderBox::peekSystemID");
    OptionalByteVector result{};
    auto peekResult = PeekBox(data, offset);
    if (!peekResult.engaged || peekResult.type != FourCC("pssh"))
        return result;
    alignas(ProtectionSystemSpecificHeaderBox) unsigned char storage[sizeof(ProtectionSystemSpecificHeaderBox)];
    auto* psshBox = reinterpret_cast<ProtectionSystemSpecificHeaderBox*>(storage);
    ConstructHeader(*psshBox);
    try {
        ParseHeader(*psshBox, data, offset);
    } catch (...) {
        DestroyHeader(*psshBox);
        throw;
    }
    new (&result.value) ByteVector(psshBox->systemID);
    result.engaged = true;
    DestroyHeader(*psshBox);
    return result;
}

}
