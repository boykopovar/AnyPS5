/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/loader/HTTPHeaderField.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/HTTPHeaderFieldExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WtfAbi::String;

HTTPHeaderField& Checked(HTTPHeaderField* self) {
    if (!self) throw std::invalid_argument("null WebCore::HTTPHeaderField");
    return *self;
}

const HTTPHeaderField& Checked(const HTTPHeaderField* self) {
    if (!self) throw std::invalid_argument("null WebCore::HTTPHeaderField");
    return *self;
}

String& Checked(String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

void Construct(HTTPHeaderField* self, String* name, String* value) {
    auto& field = Checked(self);
    auto& nameString = Checked(name);
    auto& valueString = Checked(value);
    field.name.impl = std::exchange(nameString.impl, nullptr);
    field.value.impl = std::exchange(valueString.impl, nullptr);
}

void ConstructCopy(HTTPHeaderField* self, const HTTPHeaderField* other) {
    auto& field = Checked(self);
    const auto& source = Checked(other);
    String name(source.name);
    String value(source.value);
    field.name.impl = std::exchange(name.impl, nullptr);
    field.value.impl = std::exchange(value.impl, nullptr);
}

void Destroy(HTTPHeaderField* self) {
    auto& field = Checked(self);
    String name(std::exchange(field.name.impl, nullptr));
    String value(std::exchange(field.value.impl, nullptr));
}

}

extern "C" {

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(HTTPHeaderField* self, String* name, String* value) {
    Construct(self, name, value);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1ERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2EON3WTF6StringES3_(HTTPHeaderField* self, String* name, String* value) {
    Construct(self, name, value);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2ERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD1Ev(HTTPHeaderField* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD2Ev(HTTPHeaderField* self) {
    Destroy(self);
}

HTTPHeaderField& APS5_VABI _ZN7WebCore15HTTPHeaderFieldaSERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    auto& field = Checked(self);
    const auto& source = Checked(other);
    String name(source.name);
    String value(source.value);
    String oldName(std::exchange(field.name.impl, std::exchange(name.impl, nullptr)));
    String oldValue(std::exchange(field.value.impl, std::exchange(value.impl, nullptr)));
    return field;
}

const String& APS5_VABI _ZNK7WebCore15HTTPHeaderField4nameEv(const HTTPHeaderField* self) {
    return Checked(self).name;
}

const String& APS5_VABI _ZNK7WebCore15HTTPHeaderField5valueEv(const HTTPHeaderField* self) {
    return Checked(self).value;
}

}
