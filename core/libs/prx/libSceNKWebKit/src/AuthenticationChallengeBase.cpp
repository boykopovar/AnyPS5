// Derived from Source/WebCore/platform/network/AuthenticationChallengeBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/platform/network/curl/AuthenticationChallenge.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/platform/network/curl/AuthenticationChallengeCurl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/AuthenticationChallengeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::AuthenticationChallenge;

const AuthenticationChallenge& Checked(const AuthenticationChallenge* self) {
    if (!self) throw std::invalid_argument("null WebCore::AuthenticationChallenge");
    return *self;
}

}

extern "C" {

bool APS5_VABI _ZNK7WebCore27AuthenticationChallengeBase6isNullEv(const AuthenticationChallenge* self) {
    return Checked(self).isNull != 0;
}

void APS5_VABI _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(AuthenticationChallenge* self) {
    if (!self) throw std::invalid_argument("null WebCore::AuthenticationChallenge");
    self->isNull = 1;
}

void* APS5_VABI _ZNK7WebCore23AuthenticationChallenge20authenticationClientEv(const AuthenticationChallenge*) {
    return nullptr;
}

bool APS5_VABI _ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(const AuthenticationChallenge* a, const AuthenticationChallenge* b) {
    const auto& left = Checked(a);
    const auto& right = Checked(b);
    if (left.isForMainResource != right.isForMainResource) return false;
    if (left.isInMainFrame != right.isInMainFrame) return false;
    return _ZN3WTF5equalEPKNS_10StringImplES2_(left.requestURL.impl, right.requestURL.impl);
}

}
