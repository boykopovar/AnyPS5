#pragma once

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct CacheQueryOptions {
    bool ignoreSearch;
    bool ignoreMethod;
    bool ignoreVary;
};

static_assert(offsetof(CacheQueryOptions, ignoreSearch) == 0);
static_assert(offsetof(CacheQueryOptions, ignoreVary) == 2);
static_assert(std::is_trivially_copyable_v<CacheQueryOptions>);

}  // namespace NKWebKit::WebCoreAbi
