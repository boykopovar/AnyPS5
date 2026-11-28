#pragma once

#include "prx/libSceNKWebKit/src/CrossOriginAccessControlAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct CrossOriginPreflightResultCache {
    void* preflightHashMap;
};

struct CrossOriginPreflightResultCacheItem {
    double absoluteExpiryTime;
    StoredCredentialsPolicy storedCredentialsPolicy;
    WtfAbi::StringImpl** methods;
    WtfAbi::StringImpl** headers;
};

static_assert(sizeof(CrossOriginPreflightResultCache) == 8);
static_assert(sizeof(CrossOriginPreflightResultCacheItem) == 32);
static_assert(alignof(CrossOriginPreflightResultCacheItem) == 8);
static_assert(offsetof(CrossOriginPreflightResultCacheItem, absoluteExpiryTime) == 0);
static_assert(offsetof(CrossOriginPreflightResultCacheItem, storedCredentialsPolicy) == 8);
static_assert(offsetof(CrossOriginPreflightResultCacheItem, methods) == 16);
static_assert(offsetof(CrossOriginPreflightResultCacheItem, headers) == 24);
static_assert(std::is_trivially_copyable_v<CrossOriginPreflightResultCacheItem>);

}  // namespace NKWebKit::WebCoreAbi

namespace NKWebKit::PreflightCache {

struct Item {
    double absoluteExpiryTime = 0;
    WebCoreAbi::StoredCredentialsPolicy storedCredentialsPolicy{};
    bool hasMethodsTable = false;
    std::set<std::u16string> methods;
    bool hasHeadersTable = false;
    std::set<std::u16string> headers;
};

struct Creation {
    std::unique_ptr<Item> item;
    WtfAbi::String error;
};

double LifetimeSeconds(const WtfAbi::String& maxAge);
Creation Create(WebCoreAbi::StoredCredentialsPolicy storedCredentialsPolicy, const WebCoreAbi::ResourceResponseBase& response);
std::optional<WtfAbi::String> ValidateMethodAndHeaders(const Item& item, const WtfAbi::String& method, const HttpAbi::HTTPHeaderMap& requestHeaders);
void Append(const WtfAbi::String& origin, const WtfAbi::URL& url, std::unique_ptr<Item> item);
WtfAbi::String UnsafeRequestHeaderNames(const HttpAbi::HTTPHeaderMap& requestHeaders);

}  // namespace NKWebKit::PreflightCache
