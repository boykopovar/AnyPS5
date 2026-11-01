#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSocialScreenCloseSeparateMode();
int APS5_VABI sceSocialScreenConfigureSeparateMode();
int APS5_VABI sceSocialScreenCrashDaemon();
int APS5_VABI sceSocialScreenGetIgnoreSeparateMode();
int APS5_VABI sceSocialScreenInitialize();
int APS5_VABI sceSocialScreenInitializeSeparateModeParameter(void* parameter);
int APS5_VABI sceSocialScreenOpenSeparateMode();
int APS5_VABI sceSocialScreenReserveHighFrameRateVideoCaptureRequest();
int APS5_VABI sceSocialScreenSetIgnoreSeparateMode();
int APS5_VABI sceSocialScreenSetMode();
int APS5_VABI sceSocialScreenTerminate();

}
