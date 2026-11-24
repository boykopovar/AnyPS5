// Derived from Source/WebCore/platform/network/ResourceErrorBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/InternalErrorExports.hpp"
#include "prx/libSceNKWebKit/include/LocalizedStringsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;
using NKWebKit::WtfAbi::String;

constexpr char ErrorDomain[] = "WebKitErrorDomain";
constexpr int InternalErrorCode = 300;

}

extern "C" {

ResourceError APS5_VABI _ZN7WebCore13internalErrorERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url) {
    if (!url) throw std::invalid_argument("WebCore::internalError: null WTF::URL");
    String domain = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(ErrorDomain), sizeof(ErrorDomain) - 1);
    String description = _ZN7WebCore15localizedStringEPKc("WebKit encountered an internal error");
    ResourceError error{};
    error.failingURL = *url;
    if (error.failingURL.impl) error.failingURL.impl->refCount += 2;
    error.errorCode = InternalErrorCode;
    error.type = ResourceErrorType::General;
    error.domain = std::exchange(domain.impl, nullptr);
    error.localizedDescription = std::exchange(description.impl, nullptr);
    return error;
}

}
