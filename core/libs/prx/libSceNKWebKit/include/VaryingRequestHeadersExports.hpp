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
bool APS5_VABI _ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE(
    const NKWebKit::WebCoreAbi::LegacyCookieJar* cookieJar, const NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::LegacyVaryingRequestHeader>* varyingRequestHeaders,
    const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request);
NKWebKit::WebCoreAbi::LegacySameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request);

}
