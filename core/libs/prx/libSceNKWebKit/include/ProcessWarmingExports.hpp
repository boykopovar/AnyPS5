#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ProcessWarmingAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore18PrewarmInformationC1Ev(NKWebKit::WebCoreAbi::PrewarmInformation* self);
void APS5_VABI _ZN7WebCore18PrewarmInformationC2Ev(NKWebKit::WebCoreAbi::PrewarmInformation* self);
void APS5_VABI _ZN7WebCore18PrewarmInformationC1ERKS0_(NKWebKit::WebCoreAbi::PrewarmInformation* self, const NKWebKit::WebCoreAbi::PrewarmInformation* other);
void APS5_VABI _ZN7WebCore18PrewarmInformationC2ERKS0_(NKWebKit::WebCoreAbi::PrewarmInformation* self, const NKWebKit::WebCoreAbi::PrewarmInformation* other);
NKWebKit::WebCoreAbi::PrewarmInformation& APS5_VABI _ZN7WebCore18PrewarmInformationaSERKS0_(NKWebKit::WebCoreAbi::PrewarmInformation* self, const NKWebKit::WebCoreAbi::PrewarmInformation* other);
void APS5_VABI _ZN7WebCore18PrewarmInformationD1Ev(NKWebKit::WebCoreAbi::PrewarmInformation* self);
void APS5_VABI _ZN7WebCore18PrewarmInformationD2Ev(NKWebKit::WebCoreAbi::PrewarmInformation* self);
NKWebKit::WebCoreAbi::PrewarmInformation APS5_VABI _ZN7WebCore14ProcessWarming25collectPrewarmInformationEv();
void APS5_VABI _ZN7WebCore14ProcessWarming22prewarmWithInformationERKNS_18PrewarmInformationE(const NKWebKit::WebCoreAbi::PrewarmInformation* prewarmInformation);

}
