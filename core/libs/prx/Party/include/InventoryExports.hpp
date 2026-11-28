#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI PartySetMemoryCallbacks();
int APS5_VABI PartyFinishProcessingStateChanges();
int APS5_VABI PartyNetworkGetNetworkDescriptor();
int APS5_VABI PartyCleanup();
int APS5_VABI PartyCreateLocalUser();
int APS5_VABI PartySerializeNetworkDescriptor();
int APS5_VABI PartyGetNetworks();
int APS5_VABI PartyNetworkCreateEndpoint();
int APS5_VABI PartyLocalUserGetEntityId();
int APS5_VABI PartyDeserializeNetworkDescriptor();
int APS5_VABI PartyEndpointGetDevice();
int APS5_VABI PartyEndpointSetCustomContext();
int APS5_VABI PartyStartProcessingStateChanges();
int APS5_VABI PartyNetworkGetDeviceConnectionType();
int APS5_VABI PartyNetworkAuthenticateLocalUser();
int APS5_VABI PartyInitialize();
int APS5_VABI PartyEndpointIsLocal();
int APS5_VABI PartyNetworkLeaveNetwork();
int APS5_VABI PartyLocalUserUpdateEntityToken();
int APS5_VABI PartyConnectToNetwork();
int APS5_VABI PartyCreateNewNetwork();
int APS5_VABI PartyEndpointSendMessage();
int APS5_VABI PartyNetworkGetEndpoints();
int APS5_VABI PartyGetLocalUsers();
int APS5_VABI PartyEndpointGetEntityId();
int APS5_VABI PartyEndpointGetCustomContext();

}
