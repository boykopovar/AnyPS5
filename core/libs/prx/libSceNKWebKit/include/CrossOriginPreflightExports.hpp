#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CrossOriginAccessControlAbi.hpp"
#include "prx/libSceNKWebKit/src/CrossOriginPreflightResultCache.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfExpectedAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::CrossOriginPreflightResultCache& APS5_VABI _ZN7WebCore31CrossOriginPreflightResultCache9singletonEv();
void APS5_VABI _ZN7WebCore31CrossOriginPreflightResultCache5clearEv(NKWebKit::WebCoreAbi::CrossOriginPreflightResultCache* self);
void APS5_VABI _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
    NKWebKit::WebCoreAbi::CrossOriginPreflightResultCache* self, const NKWebKit::WtfAbi::String* origin, const NKWebKit::WtfAbi::URL* url,
    NKWebKit::WebCoreAbi::CrossOriginPreflightResultCacheItem** preflightResult);
bool APS5_VABI _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
    NKWebKit::WebCoreAbi::CrossOriginPreflightResultCache* self, const NKWebKit::WtfAbi::String* origin, const NKWebKit::WtfAbi::URL* url,
    NKWebKit::WebCoreAbi::StoredCredentialsPolicy storedCredentialsPolicy, const NKWebKit::WtfAbi::String* method,
    const NKWebKit::HttpAbi::HTTPHeaderMap* requestHeaders);
NKWebKit::WtfAbi::ExpectedVoidString APS5_VABI _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request, const NKWebKit::WebCoreAbi::ResourceResponseBase* response,
    NKWebKit::WebCoreAbi::StoredCredentialsPolicy storedCredentialsPolicy, const NKWebKit::WebCoreAbi::SecurityOrigin* securityOrigin,
    const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* checkDisabler);
NKWebKit::WebCoreAbi::LegacyResourceRequestBase APS5_VABI _ZN7WebCore35createAccessControlPreflightRequestERKNS_15ResourceRequestERNS_14SecurityOriginERKN3WTF6StringE(
    const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request, NKWebKit::WebCoreAbi::SecurityOrigin* securityOrigin, const NKWebKit::WtfAbi::String* referrer);

}
