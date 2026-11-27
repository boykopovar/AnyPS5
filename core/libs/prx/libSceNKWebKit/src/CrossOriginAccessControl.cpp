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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstring>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtable;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtableStorage;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::StoredCredentialsPolicy;
using NKWebKit::WtfAbi::ExpectedVoidString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

static_assert(sizeof(bool) == 1);
static_assert(std::is_trivially_copyable_v<CrossOriginAccessControlCheckDisablerVtable>);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr std::size_t kDisablerInitializedBytes = offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled) + sizeof(bool);
static_assert(kDisablerInitializedBytes == 9);
constexpr unsigned kStringImplFlag8Bit = 4u;

struct RequestFlags {
    bool allowCookies : 1;
    bool resourceRequestUpdated : 1;
    bool platformRequestUpdated : 1;
    bool resourceRequestBodyUpdated : 1;
    bool platformRequestBodyUpdated : 1;
    bool hiddenFromInspector : 1;
    bool isTopSite : 1;
};
static_assert(sizeof(RequestFlags) == 1);
static_assert(std::is_trivially_copyable_v<RequestFlags>);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ReadDisablerRaw(const CrossOriginAccessControlCheckDisabler* self, unsigned char (&raw)[sizeof(CrossOriginAccessControlCheckDisabler)], const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    if (!GuestCopyIn_nid_no_patch(raw, self, sizeof(raw)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ValidateGuestStringImplReadable(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return;
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, guestImpl, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!header.length) return;
    if (!header.data) return;
    const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
    const std::size_t unitSize = is8Bit ? 1u : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(header.data, static_cast<std::size_t>(header.length) * unitSize, function);
}

void SetResourceRequestUpdated(const LegacyResourceRequestBase* request, const char* function) {
    constexpr std::size_t kFlagsByteOffset =
        offsetof(LegacyResourceRequestBase, inspectorInitiatorNodeIdentifier) + sizeof(NKWebKit::WebCoreAbi::LegacyOptionalInt32);
    static_assert(kFlagsByteOffset == 180);
    auto* flagByte = reinterpret_cast<unsigned char*>(const_cast<LegacyResourceRequestBase*>(request)) + kFlagsByteOffset;
    RequireReadableGuestBytes(flagByte, sizeof(RequestFlags), function);
    RequireWritableGuestBytes(flagByte, sizeof(RequestFlags), function);
    RequestFlags flags{};
    if (!GuestCopyIn_nid_no_patch(&flags, flagByte, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    flags.resourceRequestUpdated = true;
    if (!GuestCopyOut_nid_no_patch(flagByte, &flags, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
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

void ConstructDefaultDisabler(CrossOriginAccessControlCheckDisabler* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    RequireWritableGuestBytes(self, sizeof(CrossOriginAccessControlCheckDisabler), function);
    unsigned char bytes[kDisablerInitializedBytes]{};
    const CrossOriginAccessControlCheckDisablerVtable* const vptr = &Vtable.dispatch;
    std::memcpy(bytes + offsetof(CrossOriginAccessControlCheckDisabler, vptr), &vptr, sizeof(vptr));
    bytes[offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled)] = 1;
    if (!GuestCopyOut_nid_no_patch(self, bytes, sizeof(bytes)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ConstructCopyDisabler(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other, const char* function) {
    if (!self || !other) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    RequireWritableGuestBytes(self, sizeof(CrossOriginAccessControlCheckDisabler), function);
    unsigned char sourceRaw[sizeof(CrossOriginAccessControlCheckDisabler)]{};
    ReadDisablerRaw(other, sourceRaw, function);
    unsigned char bytes[kDisablerInitializedBytes]{};
    const CrossOriginAccessControlCheckDisablerVtable* const vptr = &Vtable.dispatch;
    std::memcpy(bytes + offsetof(CrossOriginAccessControlCheckDisabler, vptr), &vptr, sizeof(vptr));
    bytes[offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled)] =
        sourceRaw[offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled)] ? 1 : 0;
    if (!GuestCopyOut_nid_no_patch(self, bytes, sizeof(bytes)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

CrossOriginAccessControlCheckDisabler& AssignDisabler(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other, const char* function) {
    if (!self || !other) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    RequireWritableGuestBytes(self, sizeof(CrossOriginAccessControlCheckDisabler), function);
    unsigned char sourceRaw[sizeof(CrossOriginAccessControlCheckDisabler)]{};
    ReadDisablerRaw(other, sourceRaw, function);
    const unsigned char enabledByte =
        sourceRaw[offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled)] ? 1 : 0;
    if (!GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled),
            &enabledByte,
            sizeof(enabledByte)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return *self;
}

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

bool CheckEnabled(const CrossOriginAccessControlCheckDisabler* disabler, const char* function) {
    unsigned char raw[sizeof(CrossOriginAccessControlCheckDisabler)]{};
    ReadDisablerRaw(disabler, raw, function);
    const CrossOriginAccessControlCheckDisablerVtable* vptr = nullptr;
    std::memcpy(&vptr, raw + offsetof(CrossOriginAccessControlCheckDisabler, vptr), sizeof(vptr));
    if (!vptr)
        throw std::invalid_argument("WebCore::CrossOriginAccessControlCheckDisabler without a virtual table");
    CrossOriginAccessControlCheckDisablerVtable table{};
    if (!GuestCopyIn_nid_no_patch(&table, vptr, sizeof(table)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!table.crossOriginAccessControlCheckEnabled)
        throw std::invalid_argument("WebCore::CrossOriginAccessControlCheckDisabler without a virtual table");
    return table.crossOriginAccessControlCheckEnabled(disabler);
}

int StatusCode(const ResourceResponseBase* response, const char* function) {
    short code = 0;
    if (!GuestCopyIn_nid_no_patch(
            &code,
            reinterpret_cast<const unsigned char*>(response) + offsetof(ResourceResponseStatusLayout, httpStatusCode),
            sizeof(code)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return code;
}

ExpectedVoidString AccessControlCheck(const ResourceResponseBase* response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
                                      const CrossOriginAccessControlCheckDisabler* checkDisabler, bool reportStatusCode, const char* function) {
    RequireReadableGuestBytes(response, sizeof(ResourceResponseStatusLayout), function);
    RequireReadableGuestBytes(securityOrigin, sizeof(SecurityOrigin), function);
    const String allowedOrigin = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(response, HTTPHeaderName::AccessControlAllowOrigin);
    bool starAllowed = storedCredentialsPolicy == StoredCredentialsPolicy::DoNotUse;
    if (!starAllowed) starAllowed = checkDisabler && !CheckEnabled(checkDisabler, function);
    const bool isStar = EqualsLiteral(allowedOrigin, "*");
    if (isStar && starAllowed) return ExpectedVoidString();

    const String origin = _ZNK7WebCore14SecurityOrigin8toStringEv(securityOrigin);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(allowedOrigin.impl, origin.impl)) {
        if (isStar)
            return ExpectedVoidString(Concatenate({Literal("Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true.")}));
        if (Contains(allowedOrigin, u','))
            return ExpectedVoidString(Concatenate({Literal("Access-Control-Allow-Origin cannot contain more than one origin.")}));
        if (!reportStatusCode)
            return ExpectedVoidString(Concatenate({Literal("Origin "), NKWebKit::WtfAbi::ViewOf(origin.impl), Literal(" is not allowed by Access-Control-Allow-Origin.")}));
        const std::string statusCode = std::to_string(StatusCode(response, function));
        return ExpectedVoidString(Concatenate({Literal("Origin "), NKWebKit::WtfAbi::ViewOf(origin.impl),
                                               Literal(" is not allowed by Access-Control-Allow-Origin."), Literal(" Status code: "), Literal(statusCode)}));
    }

    if (storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        const String allowCredentials =
            _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(response, HTTPHeaderName::AccessControlAllowCredentials);
        if (!EqualsLiteral(allowCredentials, "true"))
            return ExpectedVoidString(Concatenate({Literal("Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\".")}));
    }
    return ExpectedVoidString();
}

ExpectedVoidString ValidatePreflightResponseImpl(const LegacyResourceRequestBase* request, const ResourceResponseBase* response,
                                                 StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
                                                 const CrossOriginAccessControlCheckDisabler* checkDisabler, const char* function) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (!response) throw std::invalid_argument("null WebCore::ResourceResponse");
    if (!securityOrigin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    RequireReadableGuestBytes(request, sizeof(LegacyResourceRequestBase), function);
    RequireReadableGuestBytes(response, sizeof(ResourceResponseStatusLayout), function);
    RequireReadableGuestBytes(securityOrigin, sizeof(SecurityOrigin), function);
    const int statusCode = StatusCode(response, function);
    if (statusCode < 200 || statusCode >= 300) return ExpectedVoidString(Concatenate({Literal("Preflight response is not successful")}));
    ExpectedVoidString accessControlCheckResult = AccessControlCheck(response, storedCredentialsPolicy, securityOrigin, checkDisabler, false, function);
    if (!accessControlCheckResult.hasValue) return accessControlCheckResult;
    auto result = NKWebKit::PreflightCache::Create(storedCredentialsPolicy, *response);
    if (!result.item) return ExpectedVoidString(std::move(result.error));
    const StringImpl* methodImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&methodImpl, reinterpret_cast<const unsigned char*>(request) + offsetof(LegacyResourceRequestBase, httpMethod), sizeof(methodImpl)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ValidateGuestStringImplReadable(methodImpl, function);
    const StringImpl* urlImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&urlImpl, reinterpret_cast<const unsigned char*>(request) + offsetof(LegacyResourceRequestBase, url), sizeof(urlImpl)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ValidateGuestStringImplReadable(urlImpl, function);
    SetResourceRequestUpdated(request, function);
    auto errorDescription = NKWebKit::PreflightCache::ValidateMethodAndHeaders(
        *result.item, request->httpMethod, reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap&>(request->httpHeaderFields));
    const String origin = _ZNK7WebCore14SecurityOrigin8toStringEv(securityOrigin);
    NKWebKit::PreflightCache::Append(origin, request->url, std::move(result.item));
    if (errorDescription) return ExpectedVoidString(std::move(*errorDescription));
    return ExpectedVoidString();
}

}

extern "C" {

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv() {
    static CrossOriginAccessControlCheckDisabler disabler{&Vtable.dispatch, true};
    return disabler;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(CrossOriginAccessControlCheckDisabler* self, bool enabled) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    RequireWritableGuestBytes(self, sizeof(CrossOriginAccessControlCheckDisabler), __func__);
    const unsigned char enabledByte = enabled ? 1 : 0;
    if (!GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled),
            &enabledByte,
            sizeof(enabledByte)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

bool APS5_VABI _ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(const CrossOriginAccessControlCheckDisabler* self) {
    unsigned char raw[sizeof(CrossOriginAccessControlCheckDisabler)]{};
    ReadDisablerRaw(self, raw, __func__);
    return raw[offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled)] != 0;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(CrossOriginAccessControlCheckDisabler* self) {
    ConstructDefaultDisabler(self, __func__);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(CrossOriginAccessControlCheckDisabler* self) {
    ConstructDefaultDisabler(self, __func__);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    ConstructCopyDisabler(self, other, __func__);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    ConstructCopyDisabler(self, other, __func__);
}

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    return AssignDisabler(self, other, __func__);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(CrossOriginAccessControlCheckDisabler*) {
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(self);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(CrossOriginAccessControlCheckDisabler* self) {
    if (self) RequireWritableGuestBytes(self, sizeof(CrossOriginAccessControlCheckDisabler), __func__);
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(self);
    _ZdlPv_nid_postfix(self);
}

ExpectedVoidString APS5_VABI _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const ResourceResponseBase* response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
    const CrossOriginAccessControlCheckDisabler* checkDisabler) {
    if (!response) throw std::invalid_argument("null WebCore::ResourceResponse");
    if (!securityOrigin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    return AccessControlCheck(response, storedCredentialsPolicy, securityOrigin, checkDisabler, true, __func__);
}

ExpectedVoidString APS5_VABI _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const LegacyResourceRequestBase* request, const ResourceResponseBase* response, StoredCredentialsPolicy storedCredentialsPolicy, const SecurityOrigin* securityOrigin,
    const CrossOriginAccessControlCheckDisabler* checkDisabler) {
    return ValidatePreflightResponseImpl(request, response, storedCredentialsPolicy, securityOrigin, checkDisabler, __func__);
}

}
