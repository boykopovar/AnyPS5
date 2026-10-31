#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRemoteplayIsRemoteViewering();
int APS5_VABI sceRemoteplayNotifyCanWakeup();
int APS5_VABI sceRemoteplayRegistCallback();
int APS5_VABI sceRemoteplayUnregistCallback();
int APS5_VABI sceRemoteplayDisableInputDevice(void);
int APS5_VABI sceRemoteplayEnableInputDevice(void);

}
