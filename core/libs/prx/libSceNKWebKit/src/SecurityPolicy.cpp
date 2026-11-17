/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 * Derived from Source/WebCore/page/SecurityPolicy.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba and Source/WTF/wtf/URL.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/SecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::LocalLoadPolicy;
using NKWebKit::WtfAbi::String;

LocalLoadPolicy localLoadPolicy = NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly;

bool ProtocolIs(const String& string, const char* protocol) {
    if (!string.impl) return false;
    const auto& impl = *string.impl;
    bool isLeading = true;
    for (unsigned i = 0; i < impl.length; ++i) {
        if (!impl.data) throw std::invalid_argument("null WTF character buffer");
        auto codeUnit = impl.hashAndFlags & 4 ? static_cast<const unsigned char*>(impl.data)[i] : static_cast<const char16_t*>(impl.data)[i];
        if (isLeading) {
            if (codeUnit <= ' ') continue;
            isLeading = false;
        } else {
            if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n') continue;
        }
        char expectedCharacter = *protocol++;
        if (!expectedCharacter) return codeUnit == ':';
        if ((codeUnit | 32) != expectedCharacter) return false;
    }
    return false;
}

}

namespace NKWebKit::WebCoreAbi {

bool SecurityPolicyRestrictAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForAll;
}

bool SecurityPolicyAllowSubstituteDataAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForLocalOnly;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(LocalLoadPolicy policy) {
    localLoadPolicy = policy;
}

bool APS5_VABI _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(const NKWebKit::WtfAbi::URL* url, const String* referrer) {
    if (!referrer) throw std::invalid_argument("null WTF::String reference");
    bool referrerIsSecureURL = ProtocolIs(*referrer, "https");
    bool referrerIsWebURL = referrerIsSecureURL || ProtocolIs(*referrer, "http");
    if (!referrerIsWebURL) return true;
    if (!referrerIsSecureURL) return false;
    bool urlIsSecureURL = _ZNK3WTF3URL10protocolIsENS_10StringViewE(url, {"https", 5, true});
    return !urlIsSecureURL;
}

}
