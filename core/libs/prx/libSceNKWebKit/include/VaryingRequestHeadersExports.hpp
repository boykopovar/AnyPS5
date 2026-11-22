#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/LegacyCookieJarAbi.hpp"
#include "prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

extern "C" {

NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::LegacyVaryingRequestHeader> APS5_VABI _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(
    const NKWebKit::WebCoreAbi::LegacyCookieJar* cookieJar, const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request,
    const NKWebKit::WebCoreAbi::ResourceResponseBase* response);

}
