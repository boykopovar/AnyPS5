#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include <cstdint>

extern "C" {

const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase3urlEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
void APS5_VABI _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(NKWebKit::WebCoreAbi::ResourceRequestBase* self, const NKWebKit::WtfAbi::URL* url);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase6isNullEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
bool APS5_VABI _ZNK7WebCore19ResourceRequestBase7isEmptyEv(const NKWebKit::WebCoreAbi::ResourceRequestBase* self);
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

}
