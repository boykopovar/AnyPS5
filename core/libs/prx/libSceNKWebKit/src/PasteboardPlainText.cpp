// Derived from Source/WebCore/platform/libwpe/PasteboardLibWPE.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
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
using NKWebKit::WebCoreAbi::ModulePasteboardStore;
using NKWebKit::WebCoreAbi::Pasteboard;
using NKWebKit::WebCoreAbi::PasteboardUniquePtr;
using NKWebKit::WebCoreAbi::SetPasteboardEntry;
using NKWebKit::WtfAbi::String;

Pasteboard& Checked(Pasteboard* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::Pasteboard::") + operation + ": null pasteboard");
    return *self;
}

const String& Checked(const String* value, const char* operation, const char* parameter) {
    if (!value) throw std::invalid_argument(std::string("WebCore::Pasteboard::") + operation + ": null " + parameter);
    return *value;
}

}  // namespace

extern "C" {

PasteboardUniquePtr APS5_VABI _ZN7WebCore10Pasteboard21createForCopyAndPasteEv() {
    return PasteboardUniquePtr(new Pasteboard());
}

void APS5_VABI _ZN7WebCore10Pasteboard14writePlainTextERKN3WTF6StringENS0_18SmartReplaceOptionE(Pasteboard* self, const String* text, NKWebKit::WebCoreAbi::SmartReplaceOption) {
    auto& pasteboard = Checked(self, "writePlainText");
    const String& textString = Checked(text, "writePlainText", "text");
    SetPasteboardEntry(ModulePasteboardStore().entries, u"text/plain;charset=utf-8", CopyGuestString(textString));
    ++pasteboard.changeCount;
}

}
