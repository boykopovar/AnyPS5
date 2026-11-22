#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ContentSecurityPolicyAbi.hpp"
#include "prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore21ContentSecurityPolicyC1EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(NKWebKit::WebCoreAbi::ContentSecurityPolicy* self, NKWebKit::WtfAbi::URL* protectedURL, NKWebKit::WebCoreAbi::ContentSecurityPolicyClient* client);
void APS5_VABI _ZN7WebCore21ContentSecurityPolicyC2EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(NKWebKit::WebCoreAbi::ContentSecurityPolicy* self, NKWebKit::WtfAbi::URL* protectedURL, NKWebKit::WebCoreAbi::ContentSecurityPolicyClient* client);
void APS5_VABI _ZN7WebCore21ContentSecurityPolicyD1Ev(NKWebKit::WebCoreAbi::ContentSecurityPolicy* self);
void APS5_VABI _ZN7WebCore21ContentSecurityPolicyD2Ev(NKWebKit::WebCoreAbi::ContentSecurityPolicy* self);
void APS5_VABI _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(NKWebKit::WebCoreAbi::ContentSecurityPolicy* self, const NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyResponseHeaders* headers, NKWebKit::WtfAbi::String* referrer, bool reportParsingErrors);
NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyResponseHeaders APS5_VABI _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(const NKWebKit::WebCoreAbi::ContentSecurityPolicy* self);
bool APS5_VABI _ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(const NKWebKit::WebCoreAbi::ContentSecurityPolicy* self);
void APS5_VABI _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(const NKWebKit::WebCoreAbi::ContentSecurityPolicy* self, NKWebKit::WtfAbi::URL* url, int requestType);
void APS5_VABI _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(const NKWebKit::WebCoreAbi::ContentSecurityPolicy* self, NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request, int requestType);

}
