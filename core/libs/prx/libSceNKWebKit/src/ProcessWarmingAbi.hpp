#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct FontCachePrewarmInformation {
    WtfAbi::Vector<WtfAbi::String> seenFamilies;
    WtfAbi::Vector<WtfAbi::String> fontNamesRequiringSystemFallback;
};

struct PrewarmInformation {
    FontCachePrewarmInformation fontCache;
};

static_assert(sizeof(FontCachePrewarmInformation) == 32);
static_assert(offsetof(FontCachePrewarmInformation, seenFamilies) == 0);
static_assert(offsetof(FontCachePrewarmInformation, fontNamesRequiringSystemFallback) == 16);
static_assert(sizeof(PrewarmInformation) == 32);
static_assert(offsetof(PrewarmInformation, fontCache) == 0);
static_assert(std::is_standard_layout_v<PrewarmInformation>);

}  // namespace NKWebKit::WebCoreAbi
