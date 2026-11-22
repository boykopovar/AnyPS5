// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/PublicSuffixCurl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * Derived from https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/curl/PublicSuffixCurl.cpp
 */

#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstring>
#include <libpsl.h>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

String LoadString(const String* string, const char* operation) {
    if (!string) throw std::invalid_argument(std::string(operation) + ": null string");
    StringImpl* impl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&impl, &string->impl, sizeof(impl))) throw std::invalid_argument(std::string(operation) + ": unreadable string");
    if (impl) impl->refCount += 2;
    return String(impl);
}

const psl_ctx_t* BuiltinSuffixes(const char* operation) {
    const psl_ctx_t* suffixes = psl_builtin();
    if (!suffixes) throw std::runtime_error(std::string(operation) + ": no built-in suffix list");
    return suffixes;
}

NKWebKit::WtfAbi::CString Utf8(const String& string, const char* operation) {
    auto utf8 = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&string, 0);
    if (!utf8.buffer) throw std::runtime_error(std::string(operation) + ": UTF-8 conversion failed");
    return utf8;
}

const char* Characters(const NKWebKit::WtfAbi::CString& utf8) {
    return reinterpret_cast<const char*>(utf8.buffer + 1);
}

bool IsPublicSuffix(const String& domain) {
    if (!domain.impl || !domain.impl->length) return false;
    const psl_ctx_t* suffixes = BuiltinSuffixes("WebCore::isPublicSuffix");
    const auto lowercase = _ZNK3WTF6String31convertToLowercaseWithoutLocaleEv(&domain);
    const auto utf8 = Utf8(lowercase, "WebCore::isPublicSuffix");
    return psl_is_public_suffix2(suffixes, Characters(utf8), PSL_TYPE_ANY | PSL_TYPE_NO_STAR_RULE);
}

bool ContainsOnlyASCII(const StringView& view) {
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) > 0x7f) return false;
    }
    return true;
}

bool Equals(const StringView& view, const char* literal) {
    const std::size_t length = std::strlen(literal);
    if (view.length != length) return false;
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != static_cast<unsigned char>(literal[i])) return false;
    }
    return true;
}

String TopPrivatelyControlledDomainInternal(const psl_ctx_t* suffixes, const char* domain) {
    String result;
    if (const char* topPrivateDomain = psl_registrable_domain(suffixes, domain)) _ZN3WTF6StringC1EPKc(&result, topPrivateDomain);
    return result;
}

}

extern "C" bool APS5_VABI _ZN7WebCore14isPublicSuffixERKN3WTF6StringE(const String* domain) {
    return IsPublicSuffix(LoadString(domain, "WebCore::isPublicSuffix"));
}

extern "C" String APS5_VABI _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(const String* domain) {
    constexpr const char* operation = "WebCore::topPrivatelyControlledDomain";
    String string = LoadString(domain, operation);
    if (!string.impl || !string.impl->length) return String();
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    if (!view.data) throw std::invalid_argument(std::string(operation) + ": null character buffer");
    if (_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE(view) || !ContainsOnlyASCII(view)) return string;
    auto lowercase = _ZNK3WTF6String23convertToASCIILowercaseEv(&string);
    if (Equals(NKWebKit::WtfAbi::ViewOf(lowercase.impl), "localhost")) return lowercase;
    if (IsPublicSuffix(lowercase)) return String();
    const auto utf8 = Utf8(lowercase, operation);
    const char* characters = Characters(utf8);
    std::size_t position = 0;
    while (position < utf8.buffer->length && characters[position] == '.') ++position;
    if (position == utf8.buffer->length) return String();
    return TopPrivatelyControlledDomainInternal(BuiltinSuffixes(operation), characters + position);
}
