#pragma once

#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct LegacyOptionalInt32 {
    bool engaged;
    std::int32_t value;
};

struct LegacyResourceRequestBase {
    WtfAbi::URL url;
    double timeoutInterval;
    WtfAbi::URL firstPartyForCookies;
    WtfAbi::String httpMethod;
    WtfAbi::String initiatorIdentifier;
    WtfAbi::String cachePartition;
    HTTPHeaderMap httpHeaderFields;
    Vector responseContentDispositionEncodingFallbackArray;
    void* httpBody;
    std::uint8_t cachePolicy;
    std::uint8_t sameSiteDisposition;
    std::uint8_t priority;
    std::uint8_t requester;
    LegacyOptionalInt32 inspectorInitiatorNodeIdentifier;
    bool allowCookies : 1;
    mutable bool resourceRequestUpdated : 1;
    mutable bool platformRequestUpdated : 1;
    mutable bool resourceRequestBodyUpdated : 1;
    mutable bool platformRequestBodyUpdated : 1;
    bool hiddenFromInspector : 1;
    bool isTopSite : 1;
};

static_assert(sizeof(LegacyOptionalInt32) == 8);
static_assert(offsetof(LegacyOptionalInt32, value) == 4);
static_assert(sizeof(LegacyResourceRequestBase) == 184);
static_assert(offsetof(LegacyResourceRequestBase, url) == 0);
static_assert(offsetof(LegacyResourceRequestBase, timeoutInterval) == 40);
static_assert(offsetof(LegacyResourceRequestBase, firstPartyForCookies) == 48);
static_assert(offsetof(LegacyResourceRequestBase, httpMethod) == 88);
static_assert(offsetof(LegacyResourceRequestBase, initiatorIdentifier) == 96);
static_assert(offsetof(LegacyResourceRequestBase, cachePartition) == 104);
static_assert(offsetof(LegacyResourceRequestBase, httpHeaderFields) == 112);
static_assert(offsetof(LegacyResourceRequestBase, responseContentDispositionEncodingFallbackArray) == 144);
static_assert(offsetof(LegacyResourceRequestBase, httpBody) == 160);
static_assert(offsetof(LegacyResourceRequestBase, cachePolicy) == 168);
static_assert(offsetof(LegacyResourceRequestBase, sameSiteDisposition) == 169);
static_assert(offsetof(LegacyResourceRequestBase, priority) == 170);
static_assert(offsetof(LegacyResourceRequestBase, requester) == 171);
static_assert(offsetof(LegacyResourceRequestBase, inspectorInitiatorNodeIdentifier) == 172);

}
