#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CrossOriginAccessControlAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseStatusAbi.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfExpectedAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self, bool enabled);
NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv();
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self, const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* other);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self, const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* other);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self, const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* other);
bool APS5_VABI _ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* self);
NKWebKit::WtfAbi::ExpectedVoidString APS5_VABI _ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
    const NKWebKit::WebCoreAbi::ResourceResponseBase* response, NKWebKit::WebCoreAbi::StoredCredentialsPolicy storedCredentialsPolicy,
    const NKWebKit::WebCoreAbi::SecurityOrigin* securityOrigin, const NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler* checkDisabler);

}
