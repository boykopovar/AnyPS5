#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/NicosiaSceneAbi.hpp"

extern "C" {

void APS5_VABI _ZN7Nicosia5Scene5StateC1Ev(NKWebKit::NicosiaAbi::SceneState* self);
void APS5_VABI _ZN7Nicosia5Scene5StateC2Ev(NKWebKit::NicosiaAbi::SceneState* self);
void APS5_VABI _ZN7Nicosia5Scene5StateC1ERKS1_(NKWebKit::NicosiaAbi::SceneState* self, const NKWebKit::NicosiaAbi::SceneState* other);
void APS5_VABI _ZN7Nicosia5Scene5StateC2ERKS1_(NKWebKit::NicosiaAbi::SceneState* self, const NKWebKit::NicosiaAbi::SceneState* other);
void APS5_VABI _ZN7Nicosia5Scene5StateD1Ev(NKWebKit::NicosiaAbi::SceneState* self);
void APS5_VABI _ZN7Nicosia5Scene5StateD2Ev(NKWebKit::NicosiaAbi::SceneState* self);
NKWebKit::NicosiaAbi::SceneState* APS5_VABI _ZN7Nicosia5Scene5StateaSERKS1_(NKWebKit::NicosiaAbi::SceneState* self, const NKWebKit::NicosiaAbi::SceneState* other);
void APS5_VABI _ZN7Nicosia5SceneC1Ev(NKWebKit::NicosiaAbi::Scene* self);
void APS5_VABI _ZN7Nicosia5SceneC2Ev(NKWebKit::NicosiaAbi::Scene* self);
void APS5_VABI _ZN7Nicosia5SceneD1Ev(NKWebKit::NicosiaAbi::Scene* self);
void APS5_VABI _ZN7Nicosia5SceneD2Ev(NKWebKit::NicosiaAbi::Scene* self);
NKWebKit::NicosiaAbi::SceneRef* APS5_VABI _ZN7Nicosia5Scene6createEv(NKWebKit::NicosiaAbi::SceneRef* result);
void APS5_VABI _ZN7Nicosia16SceneIntegrationC1ERNS_5SceneERNS0_6ClientE(
    NKWebKit::NicosiaAbi::SceneIntegration* self, NKWebKit::NicosiaAbi::Scene* scene, NKWebKit::NicosiaAbi::SceneIntegrationClient* client);
void APS5_VABI _ZN7Nicosia16SceneIntegrationC2ERNS_5SceneERNS0_6ClientE(
    NKWebKit::NicosiaAbi::SceneIntegration* self, NKWebKit::NicosiaAbi::Scene* scene, NKWebKit::NicosiaAbi::SceneIntegrationClient* client);
void APS5_VABI _ZN7Nicosia16SceneIntegrationD1Ev(NKWebKit::NicosiaAbi::SceneIntegration* self);
void APS5_VABI _ZN7Nicosia16SceneIntegrationD2Ev(NKWebKit::NicosiaAbi::SceneIntegration* self);
NKWebKit::NicosiaAbi::SceneIntegrationRef* APS5_VABI _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(
    NKWebKit::NicosiaAbi::SceneIntegrationRef* result, NKWebKit::NicosiaAbi::Scene* scene, NKWebKit::NicosiaAbi::SceneIntegrationClient* client);
void APS5_VABI _ZN7Nicosia16SceneIntegration9setClientERNS0_6ClientE(NKWebKit::NicosiaAbi::SceneIntegration* self, NKWebKit::NicosiaAbi::SceneIntegrationClient* client);
void APS5_VABI _ZN7Nicosia16SceneIntegration10invalidateEv(NKWebKit::NicosiaAbi::SceneIntegration* self);
void APS5_VABI _ZN7Nicosia16SceneIntegration13requestUpdateEv(NKWebKit::NicosiaAbi::SceneIntegration* self);

}
