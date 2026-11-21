// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/network/ProtectionSpaceBase.cpp
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
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/network/ProtectionSpaceBase.h
/*
 * Copyright (C) 2007-2021 Apple Inc.  All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

enum ServerType : std::uint8_t {
    HTTP = 1,
    HTTPS = 2,
    FTP = 3,
    FTPS = 4,
    ProxyHTTP = 5,
    ProxyHTTPS = 6,
    ProxyFTP = 7,
    ProxySOCKS = 8
};

enum AuthenticationScheme : std::uint8_t {
    Default = 1,
    HTTPBasic = 2,
    HTTPDigest = 3,
    HTMLForm = 4,
    NTLM = 5,
    Negotiate = 6,
    ClientCertificateRequested = 7,
    ServerTrustEvaluationRequested = 8,
    OAuth = 9,
    Unknown = 100
};

const WebCore::ProtectionSpaceBase& Checked(const WebCore::ProtectionSpaceBase* self) {
    if (!self) throw std::invalid_argument("ProtectionSpaceBase: null self");
    return *self;
}

}

extern "C" bool APS5_VABI _ZNK7WebCore19ProtectionSpaceBase7isProxyEv(const WebCore::ProtectionSpaceBase* self) {
    const auto type = Checked(self).m_serverType;
    return type == ProxyHTTP || type == ProxyHTTPS || type == ProxyFTP || type == ProxySOCKS;
}

extern "C" bool APS5_VABI _ZNK7WebCore19ProtectionSpaceBase26receivesCredentialSecurelyEv(const WebCore::ProtectionSpaceBase* self) {
    const auto& space = Checked(self);
    return space.m_serverType == HTTPS || space.m_serverType == FTPS || space.m_serverType == ProxyHTTPS || space.m_authenticationScheme == HTTPDigest;
}

extern "C" bool APS5_VABI _ZNK7WebCore19ProtectionSpaceBase15isPasswordBasedEv(const WebCore::ProtectionSpaceBase* self) {
    switch (Checked(self).m_authenticationScheme) {
    case Default:
    case HTTPBasic:
    case HTTPDigest:
    case HTMLForm:
    case NTLM:
    case Negotiate:
    case OAuth:
        return true;
    case ClientCertificateRequested:
    case ServerTrustEvaluationRequested:
    case Unknown:
        return false;
    }
    return true;
}
