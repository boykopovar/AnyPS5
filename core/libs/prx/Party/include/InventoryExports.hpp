#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI PartyNetworkGetNetworkDescriptor();
int APS5_VABI PartySerializeNetworkDescriptor();
int APS5_VABI PartyNetworkCreateEndpoint();
int APS5_VABI PartyDeserializeNetworkDescriptor();
int APS5_VABI PartyEndpointGetDevice();
int APS5_VABI PartyEndpointSetCustomContext();
int APS5_VABI PartyNetworkGetDeviceConnectionType();
int APS5_VABI PartyNetworkAuthenticateLocalUser();
int APS5_VABI PartyEndpointIsLocal();
int APS5_VABI PartyNetworkLeaveNetwork();
int APS5_VABI PartyConnectToNetwork();
int APS5_VABI PartyCreateNewNetwork();
int APS5_VABI PartyEndpointSendMessage();
int APS5_VABI PartyNetworkGetEndpoints();
int APS5_VABI PartyEndpointGetEntityId();
int APS5_VABI PartyEndpointGetCustomContext();

}
