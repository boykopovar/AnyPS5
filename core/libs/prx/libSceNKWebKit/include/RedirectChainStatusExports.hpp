#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore25updateRedirectChainStatusERNS_24RedirectChainCacheStatusERKNS_16ResourceResponseE(
    WebCore::RedirectChainCacheStatus& redirectChainCacheStatus, const NKWebKit::WebCoreAbi::ResourceResponseBase& response);

}
