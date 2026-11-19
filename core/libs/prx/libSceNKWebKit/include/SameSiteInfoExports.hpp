#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct SameSiteInfo {
    bool isSameSite;
    bool isTopSite;
    bool isSafeHTTPMethod;
};

static_assert(sizeof(SameSiteInfo) == 3);
static_assert(offsetof(SameSiteInfo, isTopSite) == 1);
static_assert(offsetof(SameSiteInfo, isSafeHTTPMethod) == 2);

}

extern "C" {

NKWebKit::WebCoreAbi::SameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const NKWebKit::WebCoreAbi::ResourceRequestBase* request);

}
