#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView host);
WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url);
WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZN7WebCore14SecurityOrigin12createUniqueEv();
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore14SecurityOrigin8toStringEv(const NKWebKit::WebCoreAbi::SecurityOrigin* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore14SecurityOrigin11toRawStringEv(const NKWebKit::WebCoreAbi::SecurityOrigin* self);
WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(const NKWebKit::WebCoreAbi::SecurityOrigin* self);
bool APS5_VABI _ZNK7WebCore14SecurityOrigin5equalEPKS0_(const NKWebKit::WebCoreAbi::SecurityOrigin* self, const NKWebKit::WebCoreAbi::SecurityOrigin* other);
bool APS5_VABI _ZNK7WebCore14SecurityOrigin14isSameOriginAsERKS0_(const NKWebKit::WebCoreAbi::SecurityOrigin* self, const NKWebKit::WebCoreAbi::SecurityOrigin* other);
bool APS5_VABI _ZNK7WebCore14SecurityOrigin20isSameSchemeHostPortERKS0_(const NKWebKit::WebCoreAbi::SecurityOrigin* self, const NKWebKit::WebCoreAbi::SecurityOrigin* other);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(const NKWebKit::WebCoreAbi::SecurityOrigin* self);
bool APS5_VABI _ZNK7WebCore14SecurityOrigin33isMatchingRegistrableDomainSuffixERKN3WTF6StringEb(const NKWebKit::WebCoreAbi::SecurityOrigin* self, const NKWebKit::WtfAbi::String* domainSuffix, bool treatIPAddressAsDomain);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore18SecurityOriginData8toStringEv(const NKWebKit::WebCoreAbi::SecurityOriginData* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(const NKWebKit::WebCoreAbi::SecurityOriginData* self);
WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZNK7WebCore18SecurityOriginData14securityOriginEv(const NKWebKit::WebCoreAbi::SecurityOriginData* self);

}
