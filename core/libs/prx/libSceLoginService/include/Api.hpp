#pragma once

#include "SceTypes.hpp"

struct LoginServiceDeviceRequest;

extern "C" {

int APS5_VABI sceLoginServiceInitialize(void);
int APS5_VABI sceLoginServiceTerminate(void);
int APS5_VABI sceLoginServiceRequestDevices(const LoginServiceDeviceRequest* param);

}
