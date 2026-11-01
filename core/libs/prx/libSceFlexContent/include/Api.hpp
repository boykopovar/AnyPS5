#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceFlexContentInitialize(void);
int APS5_VABI sceFlexContentTerminate(void);
int APS5_VABI sceFlexContentUnknown00(void);
int APS5_VABI sceFlexContentUnknown01(void);
int APS5_VABI sceFlexContentMount(void);
int APS5_VABI sceFlexContentUnknown02(void);
int APS5_VABI sceFlexContentGetDownloadProgress(void);
int APS5_VABI sceFlexContentDelete(void);
int APS5_VABI sceFlexContentGetLatestVersion(void);
int APS5_VABI sceFlexContentGetInstalledVersion(void);
int APS5_VABI sceFlexContentUnknown03(void);
int APS5_VABI sceFlexContentUnknown04(void);

}
