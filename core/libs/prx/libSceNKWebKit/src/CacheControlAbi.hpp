#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>

namespace NKWebKit::HttpAbi {

struct CacheControlDirectives {
    double maxAge = std::numeric_limits<double>::quiet_NaN();
    double maxStale = std::numeric_limits<double>::quiet_NaN();
    double staleWhileRevalidate = std::numeric_limits<double>::quiet_NaN();
    bool noCache : 1 = false;
    bool noStore : 1 = false;
    bool mustRevalidate : 1 = false;
    bool immutable : 1 = false;
};

static_assert(sizeof(CacheControlDirectives) == 32);
static_assert(alignof(CacheControlDirectives) == 8);
static_assert(offsetof(CacheControlDirectives, maxAge) == 0);
static_assert(offsetof(CacheControlDirectives, maxStale) == 8);
static_assert(offsetof(CacheControlDirectives, staleWhileRevalidate) == 16);
static_assert(std::is_trivially_copyable_v<CacheControlDirectives>);
static_assert(std::is_trivially_destructible_v<CacheControlDirectives>);

}
