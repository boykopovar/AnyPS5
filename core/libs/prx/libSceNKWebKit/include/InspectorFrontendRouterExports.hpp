#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/InspectorFrontendRouterAbi.hpp"

extern "C" {

void APS5_VABI _ZN9Inspector13AgentRegistry13discardValuesEv(NKWebKit::InspectorAbi::AgentRegistry* self);
void APS5_VABI _ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(NKWebKit::InspectorAbi::AgentRegistry* self, NKWebKit::InspectorAbi::FrontendRouter* frontendRouter, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher);
void APS5_VABI _ZN9Inspector13AgentRegistry29willDestroyFrontendAndBackendENS_16DisconnectReasonE(NKWebKit::InspectorAbi::AgentRegistry* self, NKWebKit::InspectorAbi::DisconnectReason reason);
void APS5_VABI _ZN9Inspector13AgentRegistryC2Ev(NKWebKit::InspectorAbi::AgentRegistry* self);
void APS5_VABI _ZN9Inspector13AgentRegistryD1Ev(NKWebKit::InspectorAbi::AgentRegistry* self);
void APS5_VABI _ZN9Inspector13AgentRegistryD2Ev(NKWebKit::InspectorAbi::AgentRegistry* self);
void APS5_VABI _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(NKWebKit::InspectorAbi::FrontendRouter* self, NKWebKit::InspectorAbi::FrontendChannel* connection);
void APS5_VABI _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(NKWebKit::InspectorAbi::FrontendRouter* self, NKWebKit::InspectorAbi::FrontendChannel* connection);
void APS5_VABI _ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv(NKWebKit::InspectorAbi::FrontendRouter* self);
NKWebKit::InspectorAbi::FrontendRouterRef APS5_VABI _ZN9Inspector14FrontendRouter6createEv();
void APS5_VABI _ZN9Inspector14FrontendRouterC1Ev(NKWebKit::InspectorAbi::FrontendRouter* self);
void APS5_VABI _ZN9Inspector14FrontendRouterC2Ev(NKWebKit::InspectorAbi::FrontendRouter* self);
void APS5_VABI _ZN9Inspector14FrontendRouterD1Ev(NKWebKit::InspectorAbi::FrontendRouter* self);
void APS5_VABI _ZN9Inspector14FrontendRouterD2Ev(NKWebKit::InspectorAbi::FrontendRouter* self);
bool APS5_VABI _ZNK9Inspector14FrontendRouter12hasFrontendsEv(const NKWebKit::InspectorAbi::FrontendRouter* self);
void APS5_VABI _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendRouter* self, const NKWebKit::WtfAbi::String* message);
bool APS5_VABI _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(const NKWebKit::InspectorAbi::FrontendRouter* self);
bool APS5_VABI _ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(const NKWebKit::InspectorAbi::FrontendRouter* self);
void APS5_VABI _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendRouter* self, const NKWebKit::WtfAbi::String* message);

}
