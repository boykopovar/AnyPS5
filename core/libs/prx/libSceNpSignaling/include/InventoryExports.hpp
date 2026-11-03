#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSignalingActivateConnectionA();
int APS5_VABI sceNpSignalingCancelPeerNetInfo();
int APS5_VABI sceNpSignalingCreateContextA();
int APS5_VABI sceNpSignalingGetConnectionFromNpId();
int APS5_VABI sceNpSignalingGetConnectionFromPeerAddressA();
int APS5_VABI sceNpSignalingGetConnectionFromPeerAddress();
int APS5_VABI sceNpSignalingGetConnectionInfoA();
int APS5_VABI sceNpSignalingGetConnectionStatistics();
int APS5_VABI sceNpSignalingGetConnectionStatus();
int APS5_VABI sceNpSignalingGetContextOption();
int APS5_VABI sceNpSignalingGetLocalNetInfo();
int APS5_VABI sceNpSignalingGetMemoryInfo();
int APS5_VABI sceNpSignalingGetPeerNetInfoA();
int APS5_VABI sceNpSignalingGetPeerNetInfoResult();
int APS5_VABI sceNpSignalingGetPeerNetInfo();

}
