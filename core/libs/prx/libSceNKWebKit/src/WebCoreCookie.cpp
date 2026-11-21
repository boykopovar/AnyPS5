// Derived from Source/WebCore/platform/network/Cookie.cpp@e12a212a531aeddc11259e8515671cf5188e038d
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
 *
 * Derived from Source/WebCore/platform/network/Cookie.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/WebCoreCookieExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::Cookie;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned HashFlagCount = 8;

const Cookie& Checked(const Cookie* cookie, const char* message) {
    if (!cookie) throw std::invalid_argument(message);
    return *cookie;
}

unsigned StringHash(const StringImpl* impl) {
    if (!impl) throw std::invalid_argument("WebCore::Cookie::hash requires a non-null name, domain and path");
    unsigned hash = impl->hashAndFlags >> HashFlagCount;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(impl);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK7WebCore6CookieeqERKS0_(const Cookie* self, const Cookie* other) {
    const Cookie& a = Checked(self, "WebCore::Cookie::operator== requires a cookie");
    const Cookie& b = Checked(other, "WebCore::Cookie::operator== requires a cookie");
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.name.impl, b.name.impl)
        && _ZN3WTF5equalEPKNS_10StringImplES2_(a.domain.impl, b.domain.impl)
        && _ZN3WTF5equalEPKNS_10StringImplES2_(a.path.impl, b.path.impl)
        && a.secure == b.secure;
}

unsigned APS5_VABI _ZNK7WebCore6Cookie4hashEv(const Cookie* self) {
    const Cookie& cookie = Checked(self, "WebCore::Cookie::hash requires a cookie");
    return StringHash(cookie.name.impl) + StringHash(cookie.domain.impl) + StringHash(cookie.path.impl) + cookie.secure;
}

}
