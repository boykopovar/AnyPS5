/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <stdexcept>
#include <utility>

namespace {
using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::Vector;
using CommonHeaders = Vector<NKWebKit::HttpAbi::CommonHeader>;
using UncommonHeaders = Vector<NKWebKit::HttpAbi::UncommonHeader>;
using FallbackArray = Vector<String>;

static_assert(sizeof(CommonHeaders) == sizeof(NKWebKit::HttpAbi::CommonHeadersVector));
static_assert(sizeof(UncommonHeaders) == sizeof(NKWebKit::HttpAbi::UncommonHeadersVector));
static_assert(sizeof(FallbackArray) == sizeof(NKWebKit::WebCoreAbi::Vector));

struct BodyReference {
    FormData* ptr;
    ~BodyReference() {
        if (ptr) ptr->deref();
    }
};

template<class TVector>
const TVector& Validated(const void* storage) {
    const auto& vector = *static_cast<const TVector*>(storage);
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null vector storage");
    return vector;
}

template<class TVector>
TVector& Storage(void* storage) {
    return *static_cast<TVector*>(storage);
}

void AssignURL(URL& target, const URL& source) {
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

void Assign(ResourceRequestBase& to, const ResourceRequestBase& from) {
    if (&to == &from) return;
    auto& toHeaders = to.httpHeaderFields.storage;
    const auto& fromHeaders = from.httpHeaderFields.storage;
    CommonHeaders common(Validated<CommonHeaders>(fromHeaders));
    UncommonHeaders uncommon(Validated<UncommonHeaders>(fromHeaders + sizeof(CommonHeaders)));
    FallbackArray fallback(Validated<FallbackArray>(&from.responseContentDispositionEncodingFallbackArray));
    Validated<CommonHeaders>(toHeaders);
    Validated<UncommonHeaders>(toHeaders + sizeof(CommonHeaders));
    Validated<FallbackArray>(&to.responseContentDispositionEncodingFallbackArray);

    AssignURL(to.url, from.url);
    AssignURL(to.firstPartyForCookies, from.firstPartyForCookies);
    to.timeoutInterval = from.timeoutInterval;
    to.httpMethod = from.httpMethod;
    Storage<CommonHeaders>(toHeaders).swap(common);
    Storage<UncommonHeaders>(toHeaders + sizeof(CommonHeaders)).swap(uncommon);
    Storage<FallbackArray>(&to.responseContentDispositionEncodingFallbackArray).swap(fallback);
    to.cachePolicy = from.cachePolicy;
    to.sameSiteDisposition = from.sameSiteDisposition;
    to.priority = from.priority;
    to.requester = from.requester;
    to.isTopSite = from.isTopSite;
    to.allowCookies = from.allowCookies;
    to.isAppInitiated = from.isAppInitiated;
    to.privacyProxyFailClosedForUnreachableNonMainHosts = from.privacyProxyFailClosedForUnreachableNonMainHosts;
    to.useAdvancedPrivacyProtections = from.useAdvancedPrivacyProtections;
    to.initiatorIdentifier = from.initiatorIdentifier;
    to.cachePartition = from.cachePartition;
    auto* body = static_cast<FormData*>(from.httpBody);
    if (body) body->ref();
    BodyReference previous{static_cast<FormData*>(std::exchange(to.httpBody, body))};
    to.inspectorInitiatorNodeIdentifier = from.inspectorInitiatorNodeIdentifier;
    to.resourceRequestUpdated = from.resourceRequestUpdated;
    to.platformRequestUpdated = from.platformRequestUpdated;
    to.resourceRequestBodyUpdated = from.resourceRequestBodyUpdated;
    to.platformRequestBodyUpdated = from.platformRequestBodyUpdated;
    to.hiddenFromInspector = from.hiddenFromInspector;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_(ResourceRequestBase* self, const ResourceRequestBase* delegateProvidedRequest) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (!delegateProvidedRequest) throw std::invalid_argument("null delegate WebCore::ResourceRequest");
    const auto oldPriority = _ZNK7WebCore19ResourceRequestBase8priorityEv(self);
    BodyReference oldHTTPBody{_ZNK7WebCore19ResourceRequestBase8httpBodyEv(self).ptr};
    const bool isHiddenFromInspector = self->hiddenFromInspector;
    const auto oldRequester = self->requester;
    const String oldInitiatorIdentifier = self->initiatorIdentifier;
    const auto oldInspectorInitiatorNodeIdentifier = self->inspectorInitiatorNodeIdentifier;

    Assign(*self, *delegateProvidedRequest);

    _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(self, oldPriority);
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(self, &oldHTTPBody.ptr);
    self->hiddenFromInspector = isHiddenFromInspector;
    self->requester = oldRequester;
    self->initiatorIdentifier = oldInitiatorIdentifier;
    if (oldInspectorInitiatorNodeIdentifier) self->inspectorInitiatorNodeIdentifier = *oldInspectorInitiatorNodeIdentifier;
}

}
