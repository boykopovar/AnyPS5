#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCloudClientGetStatus();
int APS5_VABI sceCloudClientInitialize();
int APS5_VABI sceCloudClientRequestClientId();
int APS5_VABI sceCloudClientSetSettings();
int APS5_VABI sceCloudClientTerminate();

}
