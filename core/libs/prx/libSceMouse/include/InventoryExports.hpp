#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMouseDisconnectAll();
int APS5_VABI sceMouseVirtualDeviceAddDevice();
int APS5_VABI sceMouseVirtualDeviceDeleteDevice();
int APS5_VABI sceMouseVirtualDeviceGetCursor();

}
