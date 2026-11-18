// Derived from Source/WebCore/html/URLDecomposition.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/URLDecompositionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::URLDecomposition;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr unsigned FullURLSlot = 2;
constexpr unsigned StringImpl8BitFlag = 4;

struct OwnedURL {
    URL url{};
    OwnedURL() = default;
    OwnedURL(const OwnedURL&) = delete;
    OwnedURL& operator=(const OwnedURL&) = delete;
    ~OwnedURL() { NKWebKit::WtfAbi::Release(url.impl); }
};

void FullURL(const URLDecomposition* self, const char* caller, OwnedURL& result) {
    if (!self) throw std::invalid_argument(std::string("URLDecomposition::") + caller + " requires an object");
    if (!self->vtable) throw std::invalid_argument(std::string("URLDecomposition::") + caller + " requires a vtable");
    using Function = void (APS5_VABI*)(URL*, const URLDecomposition*);
    reinterpret_cast<Function>(const_cast<void*>(self->vtable[FullURLSlot]))(&result.url, self);
}

char16_t Unit(StringView view, unsigned index) {
    return view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
}

String Concatenate(StringView first, StringView second) {
    const bool is8Bit = first.is8Bit && second.is8Bit;
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(first.length + second.length, is8Bit, characters));
    unsigned offset = 0;
    for (const StringView& part : {first, second}) {
        for (unsigned i = 0; i < part.length; ++i, ++offset) {
            if (is8Bit) static_cast<unsigned char*>(characters)[offset] = static_cast<unsigned char>(Unit(part, i));
            else static_cast<char16_t*>(characters)[offset] = Unit(part, i);
        }
    }
    return result;
}

String ToString(StringView view) {
    if (!view.data) return String();
    return Concatenate(view, {"", 0, true});
}

String EmptyString() {
    return String(NKWebKit::WtfAbi::EmptyStringImpl());
}

StringView WholeString(const URL& url) {
    if (!url.impl) return {nullptr, 0, true};
    return {url.impl->data, url.impl->length, !!(url.impl->hashAndFlags & StringImpl8BitFlag)};
}

std::optional<unsigned> Port(const URL& url) {
    const unsigned portLength = (url.bits >> 3) & 7;
    if (portLength < 2 || !url.impl) return std::nullopt;
    const StringView string = WholeString(url);
    if (url.hostEnd + portLength > string.length) return std::nullopt;
    unsigned value = 0;
    for (unsigned i = url.hostEnd + 1; i < url.hostEnd + portLength; ++i) {
        const char16_t digit = Unit(string, i);
        if (digit < '0' || digit > '9') return std::nullopt;
        value = value * 10 + (digit - '0');
        if (value > 0xFFFF) return std::nullopt;
    }
    return value;
}

}

extern "C" {

String APS5_VABI _ZNK7WebCore16URLDecomposition8protocolEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "protocol", fullURL);
    if (_ZN3WTF20protocolIsJavaScriptENS_10StringViewE(WholeString(fullURL.url))) return ToString({"javascript:", 11, true});
    return Concatenate(_ZNK3WTF3URL8protocolEv(&fullURL.url), {":", 1, true});
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4hostEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "host", fullURL);
    return _ZNK3WTF3URL11hostAndPortEv(&fullURL.url);
}

String APS5_VABI _ZNK7WebCore16URLDecomposition8hostnameEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "hostname", fullURL);
    return ToString(_ZNK3WTF3URL4hostEv(&fullURL.url));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4portEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "port", fullURL);
    const auto port = Port(fullURL.url);
    if (!port) return EmptyString();
    return _ZN3WTF6String6numberEj(*port);
}

String APS5_VABI _ZNK7WebCore16URLDecomposition8pathnameEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "pathname", fullURL);
    return ToString(_ZNK3WTF3URL4pathEv(&fullURL.url));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition6searchEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "search", fullURL);
    const StringView query = _ZNK3WTF3URL28queryWithLeadingQuestionMarkEv(&fullURL.url);
    return query.length <= 1 ? EmptyString() : ToString(query);
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4hashEv(const URLDecomposition* self) {
    OwnedURL fullURL;
    FullURL(self, "hash", fullURL);
    if (!_ZNK3WTF3URL18fragmentIdentifierEv(&fullURL.url).length) return EmptyString();
    return ToString(_ZNK3WTF3URL39fragmentIdentifierWithLeadingNumberSignEv(&fullURL.url));
}

}
