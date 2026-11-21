// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/ProtectionSpaceBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/ProtectionSpaceCurl.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/CertificateInfo.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/ProtectionSpaceBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/ProtectionSpaceCurl.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/CertificateInfo.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/ProtectionSpaceCompareExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::ByteVector;
using NKWebKit::WebCoreAbi::CertificateChain;
using NKWebKit::WebCoreAbi::CertificateInfo;
using NKWebKit::WebCoreAbi::ProtectionSpace;
using NKWebKit::WtfAbi::String;

enum ServerType : std::uint8_t {
    ProxyHTTP = 5,
    ProxyHTTPS = 6,
    ProxyFTP = 7,
    ProxySOCKS = 8,
};

bool Equal(const String& a, const String& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

bool Equal(const ByteVector& a, const ByteVector& b) {
    if (a.size != b.size) return false;
    return !a.size || !std::memcmp(a.buffer, b.buffer, a.size);
}

bool Equal(const CertificateChain& a, const CertificateChain& b) {
    if (a.size != b.size) return false;
    for (unsigned i = 0; i < a.size; ++i) {
        if (!Equal(a.buffer[i], b.buffer[i])) return false;
    }
    return true;
}

bool Equal(const CertificateInfo& a, const CertificateInfo& b) {
    return a.verificationError == b.verificationError
        && Equal(a.certificateChain, b.certificateChain)
        && Equal(a.privateKey, b.privateKey)
        && Equal(a.privateKeyPassword, b.privateKeyPassword);
}

bool IsProxy(const ProtectionSpace& space) {
    return space.serverType == ProxyHTTP
        || space.serverType == ProxyHTTPS
        || space.serverType == ProxyFTP
        || space.serverType == ProxySOCKS;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_(const ProtectionSpace* first, const ProtectionSpace* second) {
    if (!first || !second) throw std::invalid_argument("WebCore::ProtectionSpaceBase::compare requires two protection spaces");
    const ProtectionSpace& a = *first;
    const ProtectionSpace& b = *second;
    if (!Equal(a.host, b.host))
        return false;
    if (a.port != b.port)
        return false;
    if (a.serverType != b.serverType)
        return false;
    if (!IsProxy(a) && !Equal(a.realm, b.realm))
        return false;
    if (a.authenticationScheme != b.authenticationScheme)
        return false;
    return Equal(a.certificateInfo, b.certificateInfo);
}

}
