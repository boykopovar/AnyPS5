// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/SecurityOrigin.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityOriginFromStringExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"

#include <stdexcept>

extern "C" {

WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZN7WebCore14SecurityOrigin16createFromStringERKN3WTF6StringE(const NKWebKit::WtfAbi::String* originString) {
    if (!originString) throw std::invalid_argument("WebCore::SecurityOrigin::createFromString: null WTF::String");
    const NKWebKit::WtfAbi::URL base{};
    NKWebKit::WtfAbi::URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, originString, nullptr);
    const NKWebKit::WtfAbi::String owner(url.impl);
    return _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&url);
}

}
