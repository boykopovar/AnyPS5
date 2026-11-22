// Derived from Source/WebCore/loader/CrossOriginAccessControl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/CrossOriginAccessControl.cpp
/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/CrossOriginAccessControlExports.hpp"
#include "prx/libSceNKWebKit/include/CrossOriginPreflightExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtableStorage;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::StoredCredentialsPolicy;
using NKWebKit::WtfAbi::ExpectedVoidString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

CrossOriginAccessControlCheckDisabler& Checked(CrossOriginAccessControlCheckDisabler* self) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    return *self;
}

const CrossOriginAccessControlCheckDisabler& Checked(const CrossOriginAccessControlCheckDisabler* self) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    return *self;
}

const CrossOriginAccessControlCheckDisablerVtableStorage Vtable = {
    0,
    nullptr,
    {
        &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev,
        &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev,
        &_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv,
    },
};

StringView Literal(std::string_view text) {
    return {text.data(), static_cast<unsigned>(text.size()), true};
}

bool EqualsLiteral(const String& string, std::string_view literal) {
    if (!string.impl) return false;
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    if (view.length != literal.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(literal[index])) return false;
    }
    return true;
}

bool Contains(const String& string, char16_t unit) {
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) == unit) return true;
    }
    return false;
}

String Concatenate(std::initializer_list<StringView> parts) {
    std::uint64_t total = 0;
    bool is8Bit = true;
    for (const auto& part : parts) {
        total += part.length;
        is8Bit = is8Bit && part.is8Bit;
    }
    if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("WebCore::passesAccessControlCheck message exceeds the maximum string length");
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(total), is8Bit, characters));
    std::size_t offset = 0;
    for (const auto& part : parts) {
        for (unsigned index = 0; index < part.length; ++index, ++offset) {
            const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(part, index);
            if (is8Bit) static_cast<unsigned char*>(characters)[offset] = static_cast<unsigned char>(unit);
            else static_cast<char16_t*>(characters)[offset] = unit;
        }
    }
    return result;
}

bool CheckEnabled(const CrossOriginAccessControlCheckDisabler& disabler) {
    if (!disabler.vptr || !disabler.vptr->crossOriginAccessControlCheckEnabled)
        throw std::invalid_argument("WebCore::CrossOriginAccessControlCheckDisabler without a virtual table");
    return disabler.vptr->crossOriginAccessControlCheckEnabled(&disabler);
}

int StatusCode(const ResourceResponseBase& response) {
    return reinterpret_cast<const ResourceResponseStatusLayout&>(response).httpStatusCode;
}

ExpectedVoidString AccessControlCheck(const ResourceResponseBase& response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin& securityOrigin,
                                      const CrossOriginAccessControlCheckDisabler* checkDisabler, bool reportStatusCode) {
    const String allowedOrigin = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(&response, HTTPHeaderName::AccessControlAllowOrigin);
    bool starAllowed = storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse;
    if (!starAllowed) starAllowed = checkDisabler && !CheckEnabled(*checkDisabler);
    const bool isStar = EqualsLiteral(allowedOrigin, "*");
    if (isStar && starAllowed) return ExpectedVoidString();

    const String origin = _ZNK7WebCore14SecurityOrigin8toStringEv(&securityOrigin);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(allowedOrigin.impl, origin.impl)) {
        if (isStar)
            return ExpectedVoidString(Concatenate({Literal("Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true.")}));
        if (Contains(allowedOrigin, u','))
            return ExpectedVoidString(Concatenate({Literal("Access-Control-Allow-Origin cannot contain more than one origin.")}));
        if (!reportStatusCode)
            return ExpectedVoidString(Concatenate({Literal("Origin "), NKWebKit::WtfAbi::ViewOf(origin.impl), Literal(" is not allowed by Access-Control-Allow-Origin.")}));
        const std::string statusCode = std::to_string(StatusCode(response));
        return ExpectedVoidString(Concatenate({Literal("Origin "), NKWebKit::WtfAbi::ViewOf(origin.impl),
                                               Literal(" is not allowed by Access-Control-Allow-Origin."), Literal(" Status code: "), Literal(statusCode)}));
    }

    if (storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        const String allowCredentials =
            _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(&response, HTTPHeaderName::AccessControlAllowCredentials);
        if (!EqualsLiteral(allowCredentials, "true"))
            return ExpectedVoidString(Concatenate({Literal("Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\".")}));
    }
    return ExpectedVoidString();
}

}

extern "C" {

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv() {
    static CrossOriginAccessControlCheckDisabler disabler{&Vtable.dispatch, true};
    return disabler;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(CrossOriginAccessControlCheckDisabler* self, bool enabled) {
    Checked(self).accessControlCheckEnabled = enabled;
}

bool APS5_VABI _ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(const CrossOriginAccessControlCheckDisabler* self) {
    return Checked(self).accessControlCheckEnabled;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(CrossOriginAccessControlCheckDisabler* self) {
    auto& disabler = Checked(self);
    disabler.vptr = &Vtable.dispatch;
    disabler.accessControlCheckEnabled = true;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(self);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    auto& disabler = Checked(self);
    const auto& source = Checked(other);
    disabler.vptr = &Vtable.dispatch;
    disabler.accessControlCheckEnabled = source.accessControlCheckEnabled;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(self, other);
}

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    auto& disabler = Checked(self);
    disabler.accessControlCheckEnabled = Checked(other).accessControlCheckEnabled;
    return disabler;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(CrossOriginAccessControlCheckDisabler*) {
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(self);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(self);
    _ZdlPv_nid_postfix(self);
}

ExpectedVoidString APS5_VABI _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const ResourceResponseBase* response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
    const CrossOriginAccessControlCheckDisabler* checkDisabler) {
    if (!response) throw std::invalid_argument("null WebCore::ResourceResponse");
    if (!securityOrigin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    return AccessControlCheck(*response, storedCredentialsPolicy, *securityOrigin, checkDisabler, true);
}

ExpectedVoidString APS5_VABI _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const LegacyResourceRequestBase* request, const ResourceResponseBase* response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
    const CrossOriginAccessControlCheckDisabler* checkDisabler) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (!response) throw std::invalid_argument("null WebCore::ResourceResponse");
    if (!securityOrigin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    const int statusCode = StatusCode(*response);
    if (statusCode < 200 || statusCode >= 300) return ExpectedVoidString(Concatenate({Literal("Preflight response is not successful")}));
    ExpectedVoidString accessControlCheckResult = AccessControlCheck(*response, storedCredentialsPolicy, *securityOrigin, checkDisabler, false);
    if (!accessControlCheckResult.hasValue) return accessControlCheckResult;
    auto result = NKWebKit::PreflightCache::Create(storedCredentialsPolicy, *response);
    if (!result.item) return ExpectedVoidString(std::move(result.error));
    request->resourceRequestUpdated = true;
    auto errorDescription = NKWebKit::PreflightCache::ValidateMethodAndHeaders(
        *result.item, request->httpMethod, reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap&>(request->httpHeaderFields));
    const String origin = _ZNK7WebCore14SecurityOrigin8toStringEv(securityOrigin);
    NKWebKit::PreflightCache::Append(origin, request->url, std::move(result.item));
    if (errorDescription) return ExpectedVoidString(std::move(*errorDescription));
    return ExpectedVoidString();
}

}
