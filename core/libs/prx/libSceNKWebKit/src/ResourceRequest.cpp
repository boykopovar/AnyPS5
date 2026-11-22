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

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/FormDataExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

namespace NKWebKit::WebCoreAbi {

FormDataRefPtr::~FormDataRefPtr() { }

}

namespace {
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

double defaultTimeoutInterval = 0;

ResourceRequestBase& Checked(ResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

const ResourceRequestBase& Checked(const ResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

LegacyResourceRequestBase& Checked(LegacyResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

const LegacyResourceRequestBase& Checked(const LegacyResourceRequestBase* self) {
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

void UpdateResourceRequest(const LegacyResourceRequestBase& request) {
    if (!request.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

void UpdateResourceRequestBody(const ResourceRequestBase& request) {
    if (!request.resourceRequestBodyUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceHTTPBody is not implemented");
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

using NKWebKit::HttpAbi::HTTPHeaderName;

static_assert(sizeof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == sizeof(NKWebKit::HttpAbi::HTTPHeaderMap));
static_assert(alignof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == alignof(NKWebKit::HttpAbi::HTTPHeaderMap));

NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(ResourceRequestBase& request) {
    return *reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

const NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(const ResourceRequestBase& request) {
    return *reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

String HeaderField(const ResourceRequestBase* self, HTTPHeaderName name) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&HeaderFields(request), name);
}

void SetHeaderField(ResourceRequestBase* self, HTTPHeaderName name, const String* value, bool add) {
    auto& request = Checked(self);
    const auto& text = Checked(value);
    UpdateResourceRequest(request);
    NKWebKit::HttpAbi::UpdateCommonHeader_nid_no_patch(HeaderFields(request), name, text, add);
    request.platformRequestUpdated = false;
}

String DefaultHTTPMethod() {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("GET"), 3);
}

String IsolatedString(const String& source) {
    return _ZNKR3WTF6String12isolatedCopyEv(&source);
}

String IsolatedString(const URL& source) {
    return _ZNKR3WTF6String12isolatedCopyEv(reinterpret_cast<const String*>(&source));
}

void AssignIsolatedString(String& target, String& isolated) {
    String copy(isolated);
    std::swap(target.impl, copy.impl);
}

void AssignIsolatedURL(URL& target, String& isolated, const URL& source) {
    String copy(isolated);
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

template<class TVector>
void ValidateVector(const TVector& vector) {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null vector storage");
}

void ReleaseHeaderMap(NKWebKit::HttpAbi::HTTPHeaderMap& map) {
    ValidateVector(map.common);
    ValidateVector(map.uncommon);
    for (unsigned i = 0; i < map.common.size; ++i) NKWebKit::WtfAbi::Release(map.common.buffer[i].value.impl);
    for (unsigned i = 0; i < map.uncommon.size; ++i) {
        NKWebKit::WtfAbi::Release(map.uncommon.buffer[i].key.impl);
        NKWebKit::WtfAbi::Release(map.uncommon.buffer[i].value.impl);
    }
    _ZN3WTF8fastFreeEPv(map.common.buffer);
    _ZN3WTF8fastFreeEPv(map.uncommon.buffer);
    map.common = {};
    map.uncommon = {};
}

void AssignIsolatedHeaderMap(NKWebKit::HttpAbi::HTTPHeaderMap& target, const NKWebKit::HttpAbi::HTTPHeaderMap& source) {
    ValidateVector(source.common);
    ValidateVector(source.uncommon);
    NKWebKit::HttpAbi::HTTPHeaderMap copy{};
    copy.common.buffer = static_cast<NKWebKit::HttpAbi::CommonHeader*>(source.common.size ? _ZN3WTF10fastMallocEm(source.common.size * sizeof(NKWebKit::HttpAbi::CommonHeader)) : nullptr);
    copy.common.capacity = source.common.size;
    for (unsigned i = 0; i < source.common.size; ++i) {
        String value = IsolatedString(source.common.buffer[i].value);
        String retained(value);
        std::construct_at(copy.common.buffer + i, NKWebKit::HttpAbi::CommonHeader{source.common.buffer[i].key, std::move(retained)});
        ++copy.common.size;
    }
    copy.uncommon.buffer = static_cast<NKWebKit::HttpAbi::UncommonHeader*>(source.uncommon.size ? _ZN3WTF10fastMallocEm(source.uncommon.size * sizeof(NKWebKit::HttpAbi::UncommonHeader)) : nullptr);
    copy.uncommon.capacity = source.uncommon.size;
    for (unsigned i = 0; i < source.uncommon.size; ++i) {
        String key = IsolatedString(source.uncommon.buffer[i].key);
        String value = IsolatedString(source.uncommon.buffer[i].value);
        String retainedKey(key);
        String retainedValue(value);
        std::construct_at(copy.uncommon.buffer + i, NKWebKit::HttpAbi::UncommonHeader{std::move(retainedKey), std::move(retainedValue)});
        ++copy.uncommon.size;
    }
    ReleaseHeaderMap(target);
    target.common = copy.common;
    target.uncommon = copy.uncommon;
    copy.common = {};
    copy.uncommon = {};
}

void SetFallbackArray(ResourceRequestBase& request, const String* first, const String* second, const String* third) {
    if (!first || !second || !third) throw std::invalid_argument("null WTF::String");
    auto& vector = *reinterpret_cast<NKWebKit::WtfAbi::Vector<String>*>(&request.responseContentDispositionEncodingFallbackArray);
    if (vector.size > vector.capacity || (vector.size && !vector.buffer)) throw std::invalid_argument("invalid fallback array storage");
    UpdateResourceRequest(request);
    vector.clear();
    const unsigned count = (first->impl != nullptr) + (second->impl != nullptr) + (third->impl != nullptr);
    if (count) {
        vector.buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(count * sizeof(String)));
        vector.capacity = count;
        vector.size = 0;
    }
    for (const String* text : {first, second, third}) {
        if (!text->impl) continue;
        std::construct_at(vector.buffer + vector.size, *text);
        ++vector.size;
    }
    request.platformRequestUpdated = false;
}

bool EqualHeaderMaps(const NKWebKit::HttpAbi::HTTPHeaderMap& left, const NKWebKit::HttpAbi::HTTPHeaderMap& right) {
    ValidateVector(left.common);
    ValidateVector(left.uncommon);
    ValidateVector(right.common);
    ValidateVector(right.uncommon);
    if (left.common.size != right.common.size || left.uncommon.size != right.uncommon.size) return false;
    for (unsigned i = 0; i < left.common.size; ++i) {
        String value = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&right, left.common.buffer[i].key);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(value.impl, left.common.buffer[i].value.impl)) return false;
    }
    for (unsigned i = 0; i < left.uncommon.size; ++i) {
        String value = _ZNK7WebCore13HTTPHeaderMap17getUncommonHeaderERKN3WTF6StringE(&right, left.uncommon.buffer[i].key);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(value.impl, left.uncommon.buffer[i].value.impl)) return false;
    }
    return true;
}

constexpr std::uint8_t kBytesElement = 0;
constexpr std::uint8_t kFileElement = 1;
constexpr std::uint8_t kBlobElement = 2;
constexpr std::uint8_t kValuelessElement = 0xFF;

void SetHTTPBody(ResourceRequestBase& request, NKWebKit::WebCoreAbi::FormData* adopted) {
    UpdateResourceRequest(request);
    auto* previous = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    request.httpBody = adopted;
    if (previous) previous->deref();
    request.resourceRequestBodyUpdated = true;
    request.platformRequestBodyUpdated = false;
}

bool EqualBodyBytes(const NKWebKit::WebCoreAbi::FormDataByteVector& left, const NKWebKit::WebCoreAbi::FormDataByteVector& right) {
    if (left.size != right.size) return false;
    if (!left.size) return true;
    if (!left.buffer || !right.buffer) throw std::invalid_argument("null WebCore::FormData byte storage");
    return std::memcmp(left.buffer, right.buffer, left.size) == 0;
}

bool FormDataElementsEqual(const NKWebKit::WebCoreAbi::FormData& left, const NKWebKit::WebCoreAbi::FormData& right) {
    ValidateVector(left.elements);
    ValidateVector(right.elements);
    if (left.elements.size != right.elements.size) return false;
    for (unsigned i = 0; i < left.elements.size; ++i) {
        const auto& first = left.elements.buffer[i];
        const auto& second = right.elements.buffer[i];
        const auto kind = static_cast<std::uint8_t>(first.index & 0xFF);
        const auto otherKind = static_cast<std::uint8_t>(second.index & 0xFF);
        if (kind != otherKind) return false;
        if (kind == kBytesElement) {
            if (!EqualBodyBytes(first.storage.bytes, second.storage.bytes)) return false;
        } else if (kind == kFileElement) {
            if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.storage.file.filename, second.storage.file.filename)) return false;
            if (first.storage.file.fileStart != second.storage.file.fileStart || first.storage.file.fileLength != second.storage.file.fileLength) return false;
            if (first.storage.file.hasExpectedFileModificationTime != second.storage.file.hasExpectedFileModificationTime) return false;
            if (first.storage.file.hasExpectedFileModificationTime && first.storage.file.expectedFileModificationTime != second.storage.file.expectedFileModificationTime) return false;
        } else if (kind == kBlobElement) {
            if (!Equal(first.storage.blob.url, second.storage.blob.url)) return false;
        } else if (kind == kValuelessElement) {
            continue;
        } else {
            throw std::invalid_argument("invalid WebCore::FormDataElement index");
        }
    }
    return true;
}

void SetIsolatedCopy(ResourceRequestBase& to, const ResourceRequestBase& from) {
    UpdateResourceRequest(from);

    String url = IsolatedString(from.url);
    String firstParty = IsolatedString(from.firstPartyForCookies);
    String method = IsolatedString(from.httpMethod);
    String initiator = IsolatedString(from.initiatorIdentifier);

    UpdateResourceRequest(to);
    AssignIsolatedURL(to.url, url, from.url);
    to.platformRequestUpdated = false;
    if (to.cachePolicy != from.cachePolicy) {
        to.cachePolicy = from.cachePolicy;
        to.platformRequestUpdated = false;
    }
    if (to.timeoutInterval != from.timeoutInterval) {
        to.timeoutInterval = from.timeoutInterval;
        to.platformRequestUpdated = false;
    }
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(to.firstPartyForCookies.impl, from.firstPartyForCookies.impl)) {
        AssignIsolatedURL(to.firstPartyForCookies, firstParty, from.firstPartyForCookies);
        to.platformRequestUpdated = false;
    }
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(to.httpMethod.impl, from.httpMethod.impl)) {
        AssignIsolatedString(to.httpMethod, method);
        to.platformRequestUpdated = false;
    }
    if (to.priority != from.priority) {
        to.priority = from.priority;
        to.platformRequestUpdated = false;
    }
    to.requester = from.requester;
    AssignIsolatedString(to.initiatorIdentifier, initiator);
    if (from.inspectorInitiatorNodeIdentifier) to.inspectorInitiatorNodeIdentifier = from.inspectorInitiatorNodeIdentifier;
    if (from.sameSiteDisposition != static_cast<std::uint8_t>(NKWebKit::WebCoreAbi::Unspecified) && to.sameSiteDisposition != from.sameSiteDisposition) {
        to.sameSiteDisposition = from.sameSiteDisposition;
        to.platformRequestUpdated = false;
    }
    if (to.isTopSite != from.isTopSite) {
        to.isTopSite = from.isTopSite;
        to.platformRequestUpdated = false;
    }
    AssignIsolatedHeaderMap(HeaderFields(to), HeaderFields(from));
    const auto& source = *reinterpret_cast<const NKWebKit::WtfAbi::Vector<String>*>(&from.responseContentDispositionEncodingFallbackArray);
    if (source.size > 0) {
        if (source.size > source.capacity || !source.buffer) throw std::invalid_argument("invalid fallback array storage");
        String first = IsolatedString(source.buffer[0]);
        String second;
        String third;
        if (source.size > 1) {
            second = IsolatedString(source.buffer[1]);
            if (source.size > 2) third = IsolatedString(source.buffer[2]);
        }
        SetFallbackArray(to, &first, &second, &third);
    }
    if (from.httpBody) {
        auto isolated = _ZNK7WebCore8FormData12isolatedCopyEv(static_cast<const NKWebKit::WebCoreAbi::FormData*>(from.httpBody));
        SetHTTPBody(to, std::exchange(isolated.m_ptr, nullptr));
    }
    if (to.allowCookies != from.allowCookies) {
        to.allowCookies = from.allowCookies;
        to.platformRequestUpdated = false;
    }
    if (to.isAppInitiated != from.isAppInitiated) {
        to.isAppInitiated = from.isAppInitiated;
        to.platformRequestUpdated = false;
    }
    if (to.privacyProxyFailClosedForUnreachableNonMainHosts != from.privacyProxyFailClosedForUnreachableNonMainHosts) {
        to.privacyProxyFailClosedForUnreachableNonMainHosts = from.privacyProxyFailClosedForUnreachableNonMainHosts;
        to.platformRequestUpdated = false;
    }
    if (to.useAdvancedPrivacyProtections != from.useAdvancedPrivacyProtections) {
        to.useAdvancedPrivacyProtections = from.useAdvancedPrivacyProtections;
        to.platformRequestUpdated = false;
    }
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

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(const ResourceRequestBase* self, HTTPHeaderName name) {
    return HeaderField(self, name);
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(const ResourceRequestBase* self, HTTPHeaderName name) {
    return _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(Checked(self)), name);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase12httpReferrerEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::Referer);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::ContentType);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase13httpUserAgentEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::UserAgent);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpOriginEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::Origin);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    SetHeaderField(self, name, value, false);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(ResourceRequestBase* self, const String* name, const String* value) {
    auto& request = Checked(self);
    const auto& key = Checked(name);
    const auto& text = Checked(value);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&HeaderFields(request), key, text);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    SetHeaderField(self, name, value, true);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(ResourceRequestBase* self, const String* name, const String* value) {
    auto& request = Checked(self);
    const auto& key = Checked(name);
    const auto& text = Checked(value);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_(&HeaderFields(request), key, text);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    auto& request = Checked(self);
    const auto& text = Checked(value);
    UpdateResourceRequest(request);
    if (_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(request), name)) return;
    NKWebKit::HttpAbi::UpdateCommonHeader_nid_no_patch(HeaderFields(request), name, text, false);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(ResourceRequestBase* self, const String* referrer) {
    const auto& text = Checked(referrer);
    if (text.impl && text.impl->length > 4096) NotImplemented_nid_no_patch(__func__);
    SetHeaderField(self, HTTPHeaderName::Referer, referrer, false);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase16setHTTPUserAgentERKN3WTF6StringE(ResourceRequestBase* self, const String* userAgent) {
    SetHeaderField(self, HTTPHeaderName::UserAgent, userAgent, false);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPContentTypeERKN3WTF6StringE(ResourceRequestBase* self, const String* contentType) {
    SetHeaderField(self, HTTPHeaderName::ContentType, contentType, false);
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

void APS5_VABI _ZN7WebCore19ResourceRequestBase20clearHTTPContentTypeEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::ContentType);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    if (!_ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Authorization))
        return;
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Origin);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    constexpr HTTPHeaderName conditionalHeaders[] = {
        HTTPHeaderName::IfMatch,
        HTTPHeaderName::IfModifiedSince,
        HTTPHeaderName::IfNoneMatch,
        HTTPHeaderName::IfRange,
        HTTPHeaderName::IfUnmodifiedSince,
    };
    for (auto header : conditionalHeaders)
        _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, header);
}

void APS5_VABI WKURLRequestSetDefaultTimeoutInterval(double timeoutInterval) {
    _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(timeoutInterval);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(LegacyResourceRequestBase* self, NKWebKit::WebCoreAbi::FormData** httpBody) {
    auto& request = Checked(self);
    if (!httpBody) throw std::invalid_argument("null WTF::RefPtr");
    UpdateResourceRequest(request);
    auto* previous = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    request.httpBody = std::exchange(*httpBody, nullptr);
    if (previous) previous->deref();
    request.resourceRequestBodyUpdated = true;
    request.platformRequestBodyUpdated = false;
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(const LegacyResourceRequestBase* self, const String* name) {
    const auto& request = Checked(self);
    const auto& key = Checked(name);
    UpdateResourceRequest(request);
    return _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields), key);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase12clearPurposeEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Purpose);
    request.platformRequestUpdated = false;
}

String APS5_VABI _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(const String* domain) {
    if (!domain) throw std::invalid_argument("ResourceRequestBase::partitionName: null domain");
    if (!domain->impl) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    String highLevel = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(domain);
    if (!highLevel.impl) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    return highLevel;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Referer);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    if (!(request.url.passwordEnd > request.url.userStart)) return;
    if (request.url.bits & 1) NotImplemented_nid_no_patch(__func__);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(ResourceRequestBase* self, const ResourceRequestBase* other) {
    auto& request = Checked(self);
    const auto& source = Checked(other);
    SetIsolatedCopy(request, source);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(ResourceRequestBase*, const String*) {
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(ResourceRequestBase* self, NKWebKit::WebCoreAbi::HTTPHeaderMap* headerFields) {
    auto& request = Checked(self);
    if (!headerFields) throw std::invalid_argument("null WebCore::HTTPHeaderMap");
    auto& source = *reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(headerFields);
    ValidateVector(source.common);
    ValidateVector(source.uncommon);
    UpdateResourceRequest(request);
    ReleaseHeaderMap(HeaderFields(request));
    auto& map = HeaderFields(request);
    map.common = std::move(source.common);
    map.uncommon = std::move(source.uncommon);
    source.common = {};
    source.uncommon = {};
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    if (!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Referer)) return;
    UpdateResourceRequest(request);
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(ResourceRequestBase* self, const String* first, const String* second, const String* third) {
    auto& request = Checked(self);
    SetFallbackArray(request, first, second, third);
}

bool APS5_VABI _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(const ResourceRequestBase* left, const ResourceRequestBase* right) {
    const auto& first = Checked(left);
    const auto& second = Checked(right);
    UpdateResourceRequest(first);
    UpdateResourceRequest(second);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.url.impl, second.url.impl)) return false;
    if (first.cachePolicy != second.cachePolicy) return false;
    if (first.timeoutInterval != second.timeoutInterval) return false;
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.firstPartyForCookies.impl, second.firstPartyForCookies.impl)) return false;
    if ((first.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite) != (second.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite)) return false;
    if (first.isTopSite != second.isTopSite) return false;
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.httpMethod.impl, second.httpMethod.impl)) return false;
    if (first.allowCookies != second.allowCookies) return false;
    if (first.priority != second.priority) return false;
    if (first.requester != second.requester) return false;
    UpdateResourceRequestBody(first);
    UpdateResourceRequestBody(second);
    if (first.httpBody != second.httpBody) {
        if (!first.httpBody || !second.httpBody) return false;
        if (!FormDataElementsEqual(*static_cast<const NKWebKit::WebCoreAbi::FormData*>(first.httpBody), *static_cast<const NKWebKit::WebCoreAbi::FormData*>(second.httpBody))) return false;
    }
    return EqualHeaderMaps(HeaderFields(first), HeaderFields(second));
}

ResourceRequestBase APS5_VABI _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(const ResourceRequestBase* self) {
    const auto& source = Checked(self);
    ResourceRequestBase request{};
    request.timeoutInterval = defaultTimeoutInterval;
    request.priority = 1;
    request.allowCookies = true;
    request.isAppInitiated = true;
    request.httpMethod = DefaultHTTPMethod();
    request.cachePartition.impl = NKWebKit::WtfAbi::EmptyStringImpl();
    request.resourceRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    SetIsolatedCopy(request, source);
    return request;
}

const NKWebKit::WebCoreAbi::HTTPHeaderMap& APS5_VABI _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    return reinterpret_cast<const NKWebKit::WebCoreAbi::HTTPHeaderMap&>(HeaderFields(request));
}

NKWebKit::WebCoreAbi::FormDataRefPtr APS5_VABI _ZNK7WebCore19ResourceRequestBase8httpBodyEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    UpdateResourceRequestBody(request);
    NKWebKit::WebCoreAbi::FormDataRefPtr result;
    result.ptr = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    if (result.ptr) result.ptr->ref();
    return result;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9hasUploadEv(const ResourceRequestBase* self) {
    const auto& request = Checked(self);
    UpdateResourceRequest(request);
    UpdateResourceRequestBody(request);
    auto* body = static_cast<const NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    if (!body) return false;
    ValidateVector(body->elements);
    for (unsigned i = 0; i < body->elements.size; ++i) {
        const auto kind = static_cast<std::uint8_t>(body->elements.buffer[i].index & 0xFF);
        if (kind == kFileElement || kind == kBlobElement) return true;
        if (kind != kBytesElement && kind != kValuelessElement) throw std::invalid_argument("invalid WebCore::FormDataElement index");
    }
    return false;
}

}
