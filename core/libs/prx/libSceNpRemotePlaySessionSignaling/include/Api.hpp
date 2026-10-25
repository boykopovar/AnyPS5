#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpRemotePlaySessionSignalingCreateJoinSession();
int APS5_VABI sceNpRemotePlaySessionSignalingGetConnectionStatus();
int APS5_VABI sceNpRemotePlaySessionSignalingGetCustomProperty();
int APS5_VABI sceNpRemotePlaySessionSignalingGetLocalNetInfo();
int APS5_VABI sceNpRemotePlaySessionSignalingGetMemoryInfo();
int APS5_VABI sceNpRemotePlaySessionSignalingInitialize();
int APS5_VABI sceNpRemotePlaySessionSignalingJoinSession();
int APS5_VABI sceNpRemotePlaySessionSignalingLeaveSession();
int APS5_VABI sceNpRemotePlaySessionSignalingSetCustomProperty();
int APS5_VABI sceNpRemotePlaySessionSignalingTerminate();

}
