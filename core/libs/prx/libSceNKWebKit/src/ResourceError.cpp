// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/ResourceErrorBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/curl/ResourceErrorCurl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ResourceErrorExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

String IsolatedCopyString(StringImpl* value) {
    String source;
    source.impl = value;
    struct Unborrow {
        String& borrowed;
        ~Unborrow() { borrowed.impl = nullptr; }
    } unborrow{source};
    return _ZNKR3WTF6String12isolatedCopyEv(&source);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore17ResourceErrorBase7setTypeENS0_4TypeE(ResourceError* self, ResourceErrorType type) {
    if (!self) throw std::invalid_argument("ResourceErrorBase::setType: null this");
    self->type = type;
}

ResourceError APS5_VABI _ZNK7WebCore17ResourceErrorBase12isolatedCopyEv(const ResourceError* self) {
    if (!self) throw std::invalid_argument("ResourceErrorBase::isolatedCopy: null this");
    _ZNK7WebCore17ResourceErrorBase8lazyInitEv(self);
    String domain = IsolatedCopyString(self->domain);
    String failingURL = IsolatedCopyString(self->failingURL.impl);
    String localizedDescription = IsolatedCopyString(self->localizedDescription);
    String tlsVerificationErrorDescription = IsolatedCopyString(self->tlsVerificationErrorDescription);
    ResourceError copy{};
    copy.domain = std::exchange(domain.impl, nullptr);
    copy.errorCode = self->errorCode;
    copy.failingURL = self->failingURL;
    copy.failingURL.impl = std::exchange(failingURL.impl, nullptr);
    copy.localizedDescription = std::exchange(localizedDescription.impl, nullptr);
    copy.type = self->type;
    copy.tlsVerificationError = self->tlsVerificationError;
    copy.tlsVerificationErrorDescription = std::exchange(tlsVerificationErrorDescription.impl, nullptr);
    return copy;
}

void APS5_VABI _ZNK7WebCore17ResourceErrorBase8lazyInitEv(const ResourceError* self) {
    if (!self) throw std::invalid_argument("ResourceErrorBase::lazyInit: null this");
}

}
