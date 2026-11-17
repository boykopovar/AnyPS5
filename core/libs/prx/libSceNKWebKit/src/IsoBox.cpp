// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/IsoBoxParsingExports.hpp"

#include <stdexcept>
#include <string>

namespace {

using NKWebKit::IsoAbi::Box;
using NKWebKit::IsoAbi::FullBox;
using namespace NKWebKit::IsoParse;

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

void APS5_VABI BoxCompleteDestructor(void* self) {
    _ZN7WebCore6ISOBoxD1Ev(static_cast<Box*>(self));
}

void APS5_VABI BoxDeletingDestructor(void* self) {
    _ZN7WebCore6ISOBoxD0Ev(static_cast<Box*>(self));
}

bool APS5_VABI BoxParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore6ISOBox5parseERN3JSC8DataViewERj(static_cast<Box*>(self), view, offset);
}

void APS5_VABI FullBoxCompleteDestructor(void* self) {
    _ZN7WebCore10ISOFullBoxD1Ev(static_cast<FullBox*>(self));
}

void APS5_VABI FullBoxDeletingDestructor(void* self) {
    _ZN7WebCore10ISOFullBoxD0Ev(static_cast<FullBox*>(self));
}

bool APS5_VABI FullBoxParse(void* self, DataView* view, unsigned* offset) {
    return _ZN7WebCore10ISOFullBox5parseERN3JSC8DataViewERj(static_cast<FullBox*>(self), view, offset);
}

const Vtable BoxTable{BoxCompleteDestructor, BoxDeletingDestructor, BoxParse};
const Vtable FullBoxTable{FullBoxCompleteDestructor, FullBoxDeletingDestructor, FullBoxParse};

}  // namespace

namespace NKWebKit::IsoParse {

const Vtable* BoxVtable() {
    return &BoxTable;
}

const Vtable* FullBoxVtable() {
    return &FullBoxTable;
}

}  // namespace NKWebKit::IsoParse

extern "C" {

void* APS5_VABI _ZN7WebCore6ISOBoxnwEm10NotNullTagPv(std::size_t, int, void* location) {
    return location;
}

void APS5_VABI _ZN7WebCore6ISOBoxC1Ev(Box* self) {
    ConstructBox(Checked(self, "WebCore::ISOBox::ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxC2Ev(Box* self) {
    ConstructBox(Checked(self, "WebCore::ISOBox::ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxC1ERKS0_(Box* self, const Box* other) {
    CopyBox(Checked(self, "WebCore::ISOBox::ISOBox"), Checked(other, "WebCore::ISOBox::ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxC2ERKS0_(Box* self, const Box* other) {
    CopyBox(Checked(self, "WebCore::ISOBox::ISOBox"), Checked(other, "WebCore::ISOBox::ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxD1Ev(Box* self) {
    DestroyBox(Checked(self, "WebCore::ISOBox::~ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxD2Ev(Box* self) {
    DestroyBox(Checked(self, "WebCore::ISOBox::~ISOBox"), &BoxTable);
}

void APS5_VABI _ZN7WebCore6ISOBoxD0Ev(Box* self) {
    DestroyBox(Checked(self, "WebCore::ISOBox::~ISOBox"), &BoxTable);
    FreeBox(self);
}

bool APS5_VABI _ZN7WebCore6ISOBox4readERN3JSC8DataViewE(Box* self, DataView* view) {
    unsigned localOffset = 0;
    return VirtualParse(Checked(self, "WebCore::ISOBox::read"), CheckedView(view, "WebCore::ISOBox::read"), localOffset);
}

bool APS5_VABI _ZN7WebCore6ISOBox4readERN3JSC8DataViewERj(Box* self, DataView* view, unsigned* offset) {
    return VirtualRead(Checked(self, "WebCore::ISOBox::read"), CheckedView(view, "WebCore::ISOBox::read"), Checked(offset, "WebCore::ISOBox::read"));
}

bool APS5_VABI _ZN7WebCore6ISOBox5parseERN3JSC8DataViewERj(Box* self, DataView* view, unsigned* offset) {
    return ParseBox(Checked(self, "WebCore::ISOBox::parse"), CheckedView(view, "WebCore::ISOBox::parse"), Checked(offset, "WebCore::ISOBox::parse"));
}

PeekResult APS5_VABI _ZN7WebCore6ISOBox7peekBoxERN3JSC8DataViewEj(DataView* view, unsigned offset) {
    return PeekBox(CheckedView(view, "WebCore::ISOBox::peekBox"), offset);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxC1Ev(FullBox* self) {
    ConstructFullBox(Checked(self, "WebCore::ISOFullBox::ISOFullBox"), &FullBoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxC2Ev(FullBox* self) {
    ConstructFullBox(Checked(self, "WebCore::ISOFullBox::ISOFullBox"), &FullBoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxC1ERKS0_(FullBox* self, const FullBox* other) {
    CopyFullBox(Checked(self, "WebCore::ISOFullBox::ISOFullBox"), Checked(other, "WebCore::ISOFullBox::ISOFullBox"), &FullBoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxC2ERKS0_(FullBox* self, const FullBox* other) {
    CopyFullBox(Checked(self, "WebCore::ISOFullBox::ISOFullBox"), Checked(other, "WebCore::ISOFullBox::ISOFullBox"), &FullBoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxD1Ev(FullBox* self) {
    DestroyBox(Checked(self, "WebCore::ISOFullBox::~ISOFullBox").box, &BoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxD2Ev(FullBox* self) {
    DestroyBox(Checked(self, "WebCore::ISOFullBox::~ISOFullBox").box, &BoxTable);
}

void APS5_VABI _ZN7WebCore10ISOFullBoxD0Ev(FullBox* self) {
    DestroyBox(Checked(self, "WebCore::ISOFullBox::~ISOFullBox").box, &BoxTable);
    FreeBox(self);
}

FullBox* APS5_VABI _ZN7WebCore10ISOFullBoxaSERKS0_(FullBox* self, const FullBox* other) {
    AssignFullBox(Checked(self, "WebCore::ISOFullBox::operator="), Checked(other, "WebCore::ISOFullBox::operator="));
    return self;
}

bool APS5_VABI _ZN7WebCore10ISOFullBox5parseERN3JSC8DataViewERj(FullBox* self, DataView* view, unsigned* offset) {
    return ParseFullBox(Checked(self, "WebCore::ISOFullBox::parse"), CheckedView(view, "WebCore::ISOFullBox::parse"), Checked(offset, "WebCore::ISOFullBox::parse"));
}

}
