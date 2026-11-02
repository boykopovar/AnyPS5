#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCloudClientCancelRequestGame();
int APS5_VABI sceCloudClientReset();
int APS5_VABI sceCloudClientSetAuthCodes();
int APS5_VABI sceCloudClientTestConnection();

}
