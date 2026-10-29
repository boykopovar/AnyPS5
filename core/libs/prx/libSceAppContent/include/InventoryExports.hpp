#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAppContentCheckBundleLicenseOnDisc();
int APS5_VABI sceAppContentDownloadDataGetBlockSize();
int APS5_VABI sceAppContentGetGameTrialsFlag();
int APS5_VABI sceAppContentGetPlayableStatus();

}
