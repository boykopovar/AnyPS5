/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/HtmlSpaceTrimExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

bool IsHTMLSpace(char16_t character) {
    return character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f';
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore33stripLeadingAndTrailingHTMLSpacesERKN3WTF6StringE(const NKWebKit::WtfAbi::String& string) {
    using NKWebKit::WtfAbi::String;
    const auto* impl = string.impl;
    if (!impl) return string;
    if (!impl->length) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    if (!impl->data) throw std::invalid_argument("null WTF::String data");
    auto characterAt = [&](unsigned index) -> char16_t {
        return impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[index]
                                      : static_cast<const char16_t*>(impl->data)[index];
    };
    unsigned leading = 0;
    unsigned trailing = 0;
    while (leading < impl->length && IsHTMLSpace(characterAt(leading))) ++leading;
    if (leading == impl->length) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    while (trailing < impl->length && IsHTMLSpace(characterAt(impl->length - trailing - 1))) ++trailing;
    if (!(leading | trailing)) return string;
    return _ZN3WTF10StringImpl9substringEjj(impl, leading, impl->length - leading - trailing);
}

}
