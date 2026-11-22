// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOOriginalFormatBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOSchemeTypeBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOTrackEncryptionBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOOriginalFormatBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOSchemeTypeBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOTrackEncryptionBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/IsoProtectionBoxExports.hpp"

#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::IsoAbi::ByteVector;
using NKWebKit::IsoAbi::OptionalInt8;
using NKWebKit::IsoAbi::OriginalFormatBox;
using NKWebKit::IsoAbi::SchemeTypeBox;
using NKWebKit::IsoAbi::TrackEncryptionBox;
using namespace NKWebKit::IsoParse;

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

void APS5_VABI FormatCompleteDestructor(void* self) {
    _ZN7WebCore20ISOOriginalFormatBoxD1Ev(static_cast<OriginalFormatBox*>(self));
}

void APS5_VABI FormatDeletingDestructor(void* self) {
    _ZN7WebCore20ISOOriginalFormatBoxD0Ev(static_cast<OriginalFormatBox*>(self));
}

bool APS5_VABI FormatParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore20ISOOriginalFormatBox5parseERN3JSC8DataViewERj(static_cast<OriginalFormatBox*>(self), view, offset);
}

void APS5_VABI SchemeTypeCompleteDestructor(void* self) {
    _ZN7WebCore16ISOSchemeTypeBoxD1Ev(static_cast<SchemeTypeBox*>(self));
}

void APS5_VABI SchemeTypeDeletingDestructor(void* self) {
    _ZN7WebCore16ISOSchemeTypeBoxD0Ev(static_cast<SchemeTypeBox*>(self));
}

bool APS5_VABI SchemeTypeParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore16ISOSchemeTypeBox5parseERN3JSC8DataViewERj(static_cast<SchemeTypeBox*>(self), view, offset);
}

void APS5_VABI TrackEncryptionCompleteDestructor(void* self) {
    _ZN7WebCore21ISOTrackEncryptionBoxD1Ev(static_cast<TrackEncryptionBox*>(self));
}

void APS5_VABI TrackEncryptionDeletingDestructor(void* self) {
    _ZN7WebCore21ISOTrackEncryptionBoxD0Ev(static_cast<TrackEncryptionBox*>(self));
}

bool APS5_VABI TrackEncryptionParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore21ISOTrackEncryptionBox5parseERN3JSC8DataViewERj(static_cast<TrackEncryptionBox*>(self), view, offset);
}

const Vtable FormatTable{FormatCompleteDestructor, FormatDeletingDestructor, FormatParse};
const Vtable SchemeTypeTable{SchemeTypeCompleteDestructor, SchemeTypeDeletingDestructor, SchemeTypeParse};
const Vtable TrackEncryptionTable{TrackEncryptionCompleteDestructor, TrackEncryptionDeletingDestructor, TrackEncryptionParse};

void ConstructTrackEncryption(TrackEncryptionBox& box) {
    ConstructFullBox(box.fullBox, &TrackEncryptionTable);
    box.defaultCryptByteBlock = {0, false};
    box.defaultSkipByteBlock = {0, false};
    box.defaultIsProtected = 0;
    box.defaultPerSampleIVSize = 0;
    new (&box.defaultKID) ByteVector();
    new (&box.defaultConstantIV) ByteVector();
}

void CopyTrackEncryption(TrackEncryptionBox& box, const TrackEncryptionBox& other) {
    CopyFullBox(box.fullBox, other.fullBox, &TrackEncryptionTable);
    box.defaultCryptByteBlock = other.defaultCryptByteBlock;
    box.defaultSkipByteBlock = other.defaultSkipByteBlock;
    box.defaultIsProtected = other.defaultIsProtected;
    box.defaultPerSampleIVSize = other.defaultPerSampleIVSize;
    new (&box.defaultKID) ByteVector(other.defaultKID);
    new (&box.defaultConstantIV) ByteVector(other.defaultConstantIV);
}

void DestroyTrackEncryption(TrackEncryptionBox& box) {
    box.fullBox.box.vtable = reinterpret_cast<const void* const*>(&TrackEncryptionTable);
    Destroy(box.defaultConstantIV);
    Destroy(box.defaultKID);
    DestroyBox(box.fullBox.box, BoxVtable());
}

bool ParsePayload(TrackEncryptionBox& box, const DataView& view, unsigned& offset) {
    offset += 1;

    if (!box.fullBox.version) {
        offset += 1;
    } else {
        std::int8_t cryptAndSkip = 0;
        if (!ReadU8(view, offset, cryptAndSkip))
            return false;
        box.defaultCryptByteBlock = {static_cast<std::int8_t>(cryptAndSkip >> 4), true};
        box.defaultSkipByteBlock = {static_cast<std::int8_t>(cryptAndSkip & 0xF), true};
    }

    if (!ReadU8(view, offset, box.defaultIsProtected))
        return false;

    if (!ReadU8(view, offset, box.defaultPerSampleIVSize))
        return false;

    const void* buffer = ViewBuffer(view);
    if (!buffer)
        return false;

    auto keyID = Slice(buffer, offset, static_cast<unsigned>(offset + 16));
    offset += 16;

    Resize(box.defaultKID, 16);
    if (keyID.size() < 16)
        return false;

    std::memcpy(box.defaultKID.buffer, keyID.data(), 16);

    if (box.defaultIsProtected == 1 && !box.defaultPerSampleIVSize) {
        std::int8_t defaultConstantIVSize = 0;
        if (!ReadU8(view, offset, defaultConstantIVSize) || defaultConstantIVSize < 0)
            return false;

        ByteVector defaultConstantIV;
        ReserveInitialCapacity(defaultConstantIV, static_cast<unsigned>(defaultConstantIVSize));
        while (defaultConstantIVSize--) {
            std::int8_t character = 0;
            if (!ReadU8(view, offset, character))
                return false;
            defaultConstantIV.buffer[defaultConstantIV.size++] = static_cast<std::uint8_t>(character);
        }
        MoveAssign(box.defaultConstantIV, std::move(defaultConstantIV));
    }

    return true;
}

}  // namespace

namespace NKWebKit::IsoParse {

const Vtable* OriginalFormatBoxVtable() {
    return &FormatTable;
}

const Vtable* SchemeTypeBoxVtable() {
    return &SchemeTypeTable;
}

const Vtable* TrackEncryptionBoxVtable() {
    return &TrackEncryptionTable;
}

}  // namespace NKWebKit::IsoParse

extern "C" {

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC1Ev(OriginalFormatBox* self) {
    auto& box = Checked(self, "WebCore::ISOOriginalFormatBox::ISOOriginalFormatBox");
    ConstructBox(box.box, &FormatTable);
    box.dataFormat = 0;
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC2Ev(OriginalFormatBox* self) {
    _ZN7WebCore20ISOOriginalFormatBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC1ERKS0_(OriginalFormatBox* self, const OriginalFormatBox* other) {
    auto& box = Checked(self, "WebCore::ISOOriginalFormatBox::ISOOriginalFormatBox");
    const auto& source = Checked(other, "WebCore::ISOOriginalFormatBox::ISOOriginalFormatBox");
    CopyBox(box.box, source.box, &FormatTable);
    box.dataFormat = source.dataFormat;
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC2ERKS0_(OriginalFormatBox* self, const OriginalFormatBox* other) {
    _ZN7WebCore20ISOOriginalFormatBoxC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD1Ev(OriginalFormatBox* self) {
    auto& box = Checked(self, "WebCore::ISOOriginalFormatBox::~ISOOriginalFormatBox");
    box.box.vtable = reinterpret_cast<const void* const*>(&FormatTable);
    DestroyBox(box.box, BoxVtable());
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD2Ev(OriginalFormatBox* self) {
    _ZN7WebCore20ISOOriginalFormatBoxD1Ev(self);
}

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD0Ev(OriginalFormatBox* self) {
    _ZN7WebCore20ISOOriginalFormatBoxD1Ev(self);
    FreeBox(self);
}

OriginalFormatBox* APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxaSERKS0_(OriginalFormatBox* self, const OriginalFormatBox* other) {
    auto& box = Checked(self, "WebCore::ISOOriginalFormatBox::operator=");
    const auto& source = Checked(other, "WebCore::ISOOriginalFormatBox::operator=");
    AssignBox(box.box, source.box);
    box.dataFormat = source.dataFormat;
    return self;
}

bool APS5_VABI _ZN7WebCore20ISOOriginalFormatBox5parseERN3JSC8DataViewERj(OriginalFormatBox* self, DataView* view, unsigned* offset) {
    auto& box = Checked(self, "WebCore::ISOOriginalFormatBox::parse");
    const auto& data = CheckedView(view, "WebCore::ISOOriginalFormatBox::parse");
    auto& position = Checked(offset, "WebCore::ISOOriginalFormatBox::parse");
    if (!ParseBox(box.box, data, position))
        return false;
    return ReadU32(data, position, box.dataFormat);
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC1Ev(SchemeTypeBox* self) {
    auto& box = Checked(self, "WebCore::ISOSchemeTypeBox::ISOSchemeTypeBox");
    ConstructFullBox(box.fullBox, &SchemeTypeTable);
    box.schemeType = 0;
    box.schemeVersion = 0;
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC2Ev(SchemeTypeBox* self) {
    _ZN7WebCore16ISOSchemeTypeBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC1ERKS0_(SchemeTypeBox* self, const SchemeTypeBox* other) {
    auto& box = Checked(self, "WebCore::ISOSchemeTypeBox::ISOSchemeTypeBox");
    const auto& source = Checked(other, "WebCore::ISOSchemeTypeBox::ISOSchemeTypeBox");
    CopyFullBox(box.fullBox, source.fullBox, &SchemeTypeTable);
    box.schemeType = source.schemeType;
    box.schemeVersion = source.schemeVersion;
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC2ERKS0_(SchemeTypeBox* self, const SchemeTypeBox* other) {
    _ZN7WebCore16ISOSchemeTypeBoxC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD1Ev(SchemeTypeBox* self) {
    auto& box = Checked(self, "WebCore::ISOSchemeTypeBox::~ISOSchemeTypeBox");
    box.fullBox.box.vtable = reinterpret_cast<const void* const*>(&SchemeTypeTable);
    DestroyBox(box.fullBox.box, BoxVtable());
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD2Ev(SchemeTypeBox* self) {
    _ZN7WebCore16ISOSchemeTypeBoxD1Ev(self);
}

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD0Ev(SchemeTypeBox* self) {
    _ZN7WebCore16ISOSchemeTypeBoxD1Ev(self);
    FreeBox(self);
}

bool APS5_VABI _ZN7WebCore16ISOSchemeTypeBox5parseERN3JSC8DataViewERj(SchemeTypeBox* self, DataView* view, unsigned* offset) {
    auto& box = Checked(self, "WebCore::ISOSchemeTypeBox::parse");
    const auto& data = CheckedView(view, "WebCore::ISOSchemeTypeBox::parse");
    auto& position = Checked(offset, "WebCore::ISOSchemeTypeBox::parse");
    if (!ParseFullBox(box.fullBox, data, position))
        return false;
    if (!ReadU32(data, position, box.schemeType))
        return false;
    if (!ReadU32(data, position, box.schemeVersion))
        return false;
    return true;
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC1Ev(TrackEncryptionBox* self) {
    ConstructTrackEncryption(Checked(self, "WebCore::ISOTrackEncryptionBox::ISOTrackEncryptionBox"));
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC2Ev(TrackEncryptionBox* self) {
    _ZN7WebCore21ISOTrackEncryptionBoxC1Ev(self);
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC1ERKS0_(TrackEncryptionBox* self, const TrackEncryptionBox* other) {
    CopyTrackEncryption(Checked(self, "WebCore::ISOTrackEncryptionBox::ISOTrackEncryptionBox"), Checked(other, "WebCore::ISOTrackEncryptionBox::ISOTrackEncryptionBox"));
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC2ERKS0_(TrackEncryptionBox* self, const TrackEncryptionBox* other) {
    _ZN7WebCore21ISOTrackEncryptionBoxC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD1Ev(TrackEncryptionBox* self) {
    DestroyTrackEncryption(Checked(self, "WebCore::ISOTrackEncryptionBox::~ISOTrackEncryptionBox"));
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD2Ev(TrackEncryptionBox* self) {
    _ZN7WebCore21ISOTrackEncryptionBoxD1Ev(self);
}

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD0Ev(TrackEncryptionBox* self) {
    _ZN7WebCore21ISOTrackEncryptionBoxD1Ev(self);
    FreeBox(self);
}

TrackEncryptionBox* APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxaSERKS0_(TrackEncryptionBox* self, const TrackEncryptionBox* other) {
    auto& box = Checked(self, "WebCore::ISOTrackEncryptionBox::operator=");
    const auto& source = Checked(other, "WebCore::ISOTrackEncryptionBox::operator=");
    AssignFullBox(box.fullBox, source.fullBox);
    box.defaultCryptByteBlock = source.defaultCryptByteBlock;
    box.defaultSkipByteBlock = source.defaultSkipByteBlock;
    box.defaultIsProtected = source.defaultIsProtected;
    box.defaultPerSampleIVSize = source.defaultPerSampleIVSize;
    Assign(box.defaultKID, source.defaultKID);
    Assign(box.defaultConstantIV, source.defaultConstantIV);
    return self;
}

bool APS5_VABI _ZN7WebCore21ISOTrackEncryptionBox5parseERN3JSC8DataViewERj(TrackEncryptionBox* self, DataView* view, unsigned* offset) {
    auto& box = Checked(self, "WebCore::ISOTrackEncryptionBox::parse");
    const auto& data = CheckedView(view, "WebCore::ISOTrackEncryptionBox::parse");
    auto& position = Checked(offset, "WebCore::ISOTrackEncryptionBox::parse");
    if (!ParseFullBox(box.fullBox, data, position))
        return false;
    return ParsePayload(box, data, position);
}

}
