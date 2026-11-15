#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace NKWebKit::WebCoreAbi {

struct HTTPHeaderMap {
    alignas(8) unsigned char storage[32];
};

struct Vector {
    void* buffer;
    unsigned capacity;
    unsigned size;
};

struct ResourceRequestBase {
    WtfAbi::URL url;
    WtfAbi::URL firstPartyForCookies;
    double timeoutInterval;
    WtfAbi::String httpMethod;
    HTTPHeaderMap httpHeaderFields;
    Vector responseContentDispositionEncodingFallbackArray;
    std::uint8_t cachePolicy;
    std::uint8_t sameSiteDisposition;
    std::uint8_t priority;
    std::uint8_t requester;
    bool isTopSite : 1;
    bool allowCookies : 1;
    bool isAppInitiated : 1;
    bool privacyProxyFailClosedForUnreachableNonMainHosts : 1;
    bool useAdvancedPrivacyProtections : 1;
    WtfAbi::String initiatorIdentifier;
    WtfAbi::String cachePartition;
    void* httpBody;
    std::optional<int> inspectorInitiatorNodeIdentifier;
    mutable bool resourceRequestUpdated : 1;
    mutable bool platformRequestUpdated : 1;
    mutable bool resourceRequestBodyUpdated : 1;
    mutable bool platformRequestBodyUpdated : 1;
    bool hiddenFromInspector : 1;
};

enum SameSiteDisposition : std::uint8_t { Unspecified, SameSite, CrossSite };

static_assert(sizeof(HTTPHeaderMap) == 32);
static_assert(sizeof(Vector) == 16);
static_assert(offsetof(ResourceRequestBase, url) == 0);
static_assert(offsetof(ResourceRequestBase, firstPartyForCookies) == 40);
static_assert(offsetof(ResourceRequestBase, timeoutInterval) == 80);
static_assert(offsetof(ResourceRequestBase, httpMethod) == 88);
static_assert(offsetof(ResourceRequestBase, httpHeaderFields) == 96);
static_assert(offsetof(ResourceRequestBase, responseContentDispositionEncodingFallbackArray) == 128);
static_assert(offsetof(ResourceRequestBase, cachePolicy) == 144);
static_assert(offsetof(ResourceRequestBase, sameSiteDisposition) == 145);
static_assert(offsetof(ResourceRequestBase, priority) == 146);
static_assert(offsetof(ResourceRequestBase, requester) == 147);
static_assert(offsetof(ResourceRequestBase, initiatorIdentifier) == 152);
static_assert(offsetof(ResourceRequestBase, cachePartition) == 160);
static_assert(offsetof(ResourceRequestBase, httpBody) == 168);

}
