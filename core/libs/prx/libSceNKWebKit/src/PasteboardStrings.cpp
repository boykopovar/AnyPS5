// Derived from Source/WebCore/platform/Pasteboard.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/platform/libwpe/PasteboardLibWPE.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2015 Igalia S.L.
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

#include "prx/libSceNKWebKit/include/PasteboardExports.hpp"
#include "prx/libSceNKWebKit/src/PasteboardModel.hpp"

#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::CopyGuestString;
using NKWebKit::WebCoreAbi::CreateGuestString;
using NKWebKit::WebCoreAbi::FindPasteboardEntry;
using NKWebKit::WebCoreAbi::ModulePasteboardStore;
using NKWebKit::WebCoreAbi::Pasteboard;
using NKWebKit::WebCoreAbi::SetPasteboardEntry;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::Vector;

Pasteboard& Checked(Pasteboard* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::Pasteboard::") + operation + ": null pasteboard");
    return *self;
}

const String& Checked(const String* value, const char* operation, const char* parameter) {
    if (!value) throw std::invalid_argument(std::string("WebCore::Pasteboard::") + operation + ": null " + parameter);
    return *value;
}

bool IsEmpty(const String& value) {
    return !value.impl || !value.impl->length;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore10Pasteboard11writeStringERKN3WTF6StringES4_(Pasteboard* self, const String* type, const String* data) {
    auto& pasteboard = Checked(self, "writeString");
    const String& typeString = Checked(type, "writeString", "type");
    const String& dataString = Checked(data, "writeString", "data");
    SetPasteboardEntry(ModulePasteboardStore().entries, CopyGuestString(typeString), CopyGuestString(dataString));
    ++pasteboard.changeCount;
}

String APS5_VABI _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(Pasteboard* self, const String* type) {
    Checked(self, "readString");
    const String& typeString = Checked(type, "readString", "type");
    const auto* entry = FindPasteboardEntry(ModulePasteboardStore().entries, CopyGuestString(typeString));
    if (!entry || entry->value.empty()) return String();
    return CreateGuestString(entry->value);
}

Vector<String> APS5_VABI _ZN7WebCore10Pasteboard14readAllStringsERKN3WTF6StringE(Pasteboard* self, const String* type) {
    Vector<String> result;
    String value = _ZN7WebCore10Pasteboard10readStringERKN3WTF6StringE(self, type);
    if (IsEmpty(value)) return result;
    result.append(value);
    return result;
}

}
