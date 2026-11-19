/*
 * Derived from Source/WebCore/platform/network/SameSiteInfo.cpp at https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/SameSiteInfo.cpp
 *
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

/*
 * Derived from Source/WebCore/platform/network/HTTPParsers.cpp at https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/HTTPParsers.cpp
 *
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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

#include "prx/libSceNKWebKit/include/SameSiteInfoExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;

bool EqualIgnoringASCIICase(const String& value, std::string_view expected) {
    if (!value.impl) return false;
    const auto& impl = *value.impl;
    if (impl.length != expected.size()) return false;
    if (impl.length && !impl.data) throw std::invalid_argument("null WTF::StringImpl characters");
    for (unsigned i = 0; i < impl.length; ++i) {
        char16_t c = impl.hashAndFlags & 4
            ? static_cast<const unsigned char*>(impl.data)[i]
            : static_cast<const char16_t*>(impl.data)[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        char16_t e = static_cast<unsigned char>(expected[i]);
        if (e >= 'A' && e <= 'Z') e += 'a' - 'A';
        if (c != e) return false;
    }
    return true;
}

bool IsSafeMethod(const String& method) {
    constexpr std::array<std::string_view, 4> safeMethods { "GET", "HEAD", "OPTIONS", "TRACE" };
    for (auto value : safeMethods) {
        if (EqualIgnoringASCIICase(method, value)) return true;
    }
    return false;
}

}

extern "C" {

NKWebKit::WebCoreAbi::SameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const NKWebKit::WebCoreAbi::ResourceRequestBase* request) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    return {
        _ZNK7WebCore19ResourceRequestBase10isSameSiteEv(request),
        _ZNK7WebCore19ResourceRequestBase9isTopSiteEv(request),
        IsSafeMethod(_ZNK7WebCore19ResourceRequestBase10httpMethodEv(request)),
    };
}

}
