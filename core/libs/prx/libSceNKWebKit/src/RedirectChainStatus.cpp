// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/CacheValidation.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/RedirectChainStatusExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

extern "C" {

WTF::Seconds APS5_VABI _ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE(
    const NKWebKit::WebCoreAbi::ResourceResponseBase& response, WTF::WallTime responseTime);
WTF::Seconds APS5_VABI _ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE(
    const NKWebKit::WebCoreAbi::ResourceResponseBase& response, WTF::WallTime responseTime);

void APS5_VABI _ZN7WebCore25updateRedirectChainStatusERNS_24RedirectChainCacheStatusERKNS_16ResourceResponseE(
    WebCore::RedirectChainCacheStatus& redirectChainCacheStatus, const NKWebKit::WebCoreAbi::ResourceResponseBase& response) {
    using Status = WebCore::RedirectChainCacheStatus::Status;
    if (redirectChainCacheStatus.status == Status::NotCachedRedirection) return;
    if (_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoStoreEv(&response) ||
        _ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(&response) ||
        _ZNK7WebCore20ResourceResponseBase34cacheControlContainsMustRevalidateEv(&response)) {
        redirectChainCacheStatus.status = Status::NotCachedRedirection;
        return;
    }

    redirectChainCacheStatus.status = Status::CachedRedirection;
    const auto responseTimestamp = _ZN3WTF8WallTime3nowEv();
    const auto freshness =
        _ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE(response, responseTimestamp);
    const auto age = _ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE(response, responseTimestamp);
    const WTF::WallTime endOfValidity{responseTimestamp.m_value + freshness.m_value - age.m_value};
    if (endOfValidity.m_value < redirectChainCacheStatus.endOfValidity.m_value) redirectChainCacheStatus.endOfValidity = endOfValidity;
}

}
