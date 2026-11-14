/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;
using NKWebKit::HttpAbi::HTTPHeaderName;

static_assert(sizeof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == sizeof(NKWebKit::HttpAbi::HTTPHeaderMap));
static_assert(alignof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == alignof(NKWebKit::HttpAbi::HTTPHeaderMap));

double defaultTimeoutInterval = 0;

ResourceRequestBase& Checked(ResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

const ResourceRequestBase& Checked(const ResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

const URL& Checked(const URL* url) {
    if (!url) throw std::invalid_argument("null WTF::URL");
    return *url;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

void UpdateResourceRequest(const ResourceRequestBase& request) {
    if (!request.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

void Assign(String& target, const String& source) {
    String copy(source);
    std::swap(target.impl, copy.impl);
}

void Assign(URL& target, const URL& source) {
    String copy(source.impl);
    if (copy.impl) copy.impl->refCount += 2;
    std::swap(target.impl, copy.impl);
    target.bits = source.bits;
    target.userStart = source.userStart;
    target.userEnd = source.userEnd;
    target.passwordEnd = source.passwordEnd;
    target.hostEnd = source.hostEnd;
    target.pathAfterLastSlash = source.pathAfterLastSlash;
    target.pathEnd = source.pathEnd;
    target.queryEnd = source.queryEnd;
}

bool Equal(const URL& a, const URL& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

bool Equal(const String& a, const String& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

}

extern "C" {

const URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase3urlEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.url;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(ResourceRequestBase* self, const URL* url) {
    auto& request = Checked(self);
    const auto& value = Checked(url);
    UpdateResourceRequest(request);
    Assign(request.url, value);
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase6isNullEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return !request.url.impl;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase7isEmptyEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return !request.url.impl || !request.url.impl->length;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase13isConditionalEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    constexpr HTTPHeaderName conditionalHeaders[] = {
        HTTPHeaderName::IfMatch,
        HTTPHeaderName::IfModifiedSince,
        HTTPHeaderName::IfNoneMatch,
        HTTPHeaderName::IfRange,
        HTTPHeaderName::IfUnmodifiedSince,
    };
    for (auto header : conditionalHeaders) {
        auto* headers = reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
        if (_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, header))
            return true;
    }
    return false;
}

const URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase20firstPartyForCookiesEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.firstPartyForCookies;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(ResourceRequestBase* self, const URL* url) {
    auto& request = Checked(self);
    const auto& value = Checked(url);
    UpdateResourceRequest(request);
    if (Equal(request.firstPartyForCookies, value)) return;
    Assign(request.firstPartyForCookies, value);
    request.platformRequestUpdated = false;
}

const String& APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpMethodEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.httpMethod;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(ResourceRequestBase* self, const String* method) {
    auto& request = Checked(self);
    const auto& value = Checked(method);
    UpdateResourceRequest(request);
    if (Equal(request.httpMethod, value)) return;
    Assign(request.httpMethod, value);
    request.platformRequestUpdated = false;
}

double APS5_VABI _ZNK7WebCore19ResourceRequestBase15timeoutIntervalEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.timeoutInterval;
}

std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase11cachePolicyEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.cachePolicy;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase14setCachePolicyENS_26ResourceRequestCachePolicyE(ResourceRequestBase* self, std::uint8_t cachePolicy) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    if (request.cachePolicy == cachePolicy) return;
    request.cachePolicy = cachePolicy;
    request.platformRequestUpdated = false;
}

std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase8priorityEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.priority;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(ResourceRequestBase* self, std::uint8_t priority) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    if (request.priority == priority) return;
    request.priority = priority;
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase10isSameSiteEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(ResourceRequestBase* self, bool isSameSite) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto disposition = isSameSite ? NKWebKit::WebCoreAbi::SameSite : NKWebKit::WebCoreAbi::CrossSite;
    if (request.sameSiteDisposition == disposition) return;
    request.sameSiteDisposition = disposition;
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9isTopSiteEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return request.isTopSite;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(ResourceRequestBase* self, bool isTopSite) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    if (request.isTopSite == isTopSite) return;
    request.isTopSite = isTopSite;
    request.platformRequestUpdated = false;
}

double APS5_VABI _ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() {
    return defaultTimeoutInterval;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(double timeoutInterval) {
    defaultTimeoutInterval = timeoutInterval;
}

}
