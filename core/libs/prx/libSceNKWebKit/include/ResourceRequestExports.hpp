#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/FormDataAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include <cstdint>

extern "C" {

const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase3urlEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::URL* url);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase6isNullEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase7isEmptyEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase20firstPartyForCookiesEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::URL* url);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpMethodEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* method);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(const NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::HttpAbi::HTTPHeaderName name);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(const NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::HttpAbi::HTTPHeaderName name);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase12httpReferrerEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase13httpUserAgentEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpOriginEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::HttpAbi::HTTPHeaderName name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::HttpAbi::HTTPHeaderName name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::HttpAbi::HTTPHeaderName name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* referrer);
void APS5_VABI _ZN7WebCore19ResourceRequestBase16setHTTPUserAgentERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* userAgent);
void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPContentTypeERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* contentType);
double APS5_VABI _ZNK7WebCore19ResourceRequestBase15timeoutIntervalEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase11cachePolicyEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase14setCachePolicyENS_26ResourceRequestCachePolicyE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, std::uint8_t cachePolicy);
std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase8priorityEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, std::uint8_t priority);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase10isSameSiteEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(NKWebKit::WebCoreAbi::ResourceRequestBase* self, bool isSameSite);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9isTopSiteEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(NKWebKit::WebCoreAbi::ResourceRequestBase* self, bool isTopSite);
double APS5_VABI _ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv();
void APS5_VABI _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(double timeoutInterval);
void APS5_VABI _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::WebCoreAbi::FormData** httpBody);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(const NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* name);
void APS5_VABI _ZN7WebCore19ResourceRequestBase12clearPurposeEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(const NKWebKit::WtfAbi::String* domain);
void APS5_VABI _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WebCoreAbi::ResourceRequestBase* other);
void APS5_VABI _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* cachePartition);
void APS5_VABI _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, NKWebKit::WebCoreAbi::HTTPHeaderMap* headerFields);
void APS5_VABI _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::String* first, const NKWebKit::WtfAbi::String* second, const NKWebKit::WtfAbi::String* third);
bool APS5_VABI _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(const NKWebKit::WebCoreAbi::ResourceRequestBase* left, const NKWebKit::WebCoreAbi::ResourceRequestBase* right);
NKWebKit::WebCoreAbi::ResourceRequestBase APS5_VABI _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
const NKWebKit::WebCoreAbi::HTTPHeaderMap& APS5_VABI _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
NKWebKit::WebCoreAbi::FormDataRefPtr APS5_VABI _ZNK7WebCore19ResourceRequestBase8httpBodyEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9hasUploadEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(NKWebKit::WebCoreAbi::ResourceRequestBase* self);

}
