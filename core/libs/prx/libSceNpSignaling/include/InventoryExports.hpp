#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSignalingGetLocalNetInfo();
int APS5_VABI sceNpSignalingGetMemoryInfo();
int APS5_VABI sceNpSignalingGetPeerNetInfo();

}
