#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePatchCheckerCancel();
int APS5_VABI scePatchCheckerCheckPatch();
int APS5_VABI scePatchCheckerCreateHandler();
int APS5_VABI scePatchCheckerDestroyHandler();
int APS5_VABI scePatchCheckerDisableAutoDownload();
int APS5_VABI scePatchCheckerEnableAutoDownload();
int APS5_VABI scePatchCheckerGetApplicableTick();
int APS5_VABI scePatchCheckerGetPackageInfo();
int APS5_VABI scePatchCheckerInitialize();
int APS5_VABI scePatchCheckerRequestCheckPatch();
int APS5_VABI scePatchCheckerRequestCheckPatchByType();
int APS5_VABI scePatchCheckerTerminate();

}
