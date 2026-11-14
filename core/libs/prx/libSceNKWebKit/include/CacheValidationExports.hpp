#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore38updateResponseHeadersAfterRevalidationERNS_16ResourceResponseERKS0_(NKWebKit::WebCoreAbi::ResourceResponseBase& response, const NKWebKit::WebCoreAbi::ResourceResponseBase& validatingResponse);
WTF::Seconds APS5_VABI _ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE(const NKWebKit::WebCoreAbi::ResourceResponseBase& response, WTF::WallTime responseTime);

}
