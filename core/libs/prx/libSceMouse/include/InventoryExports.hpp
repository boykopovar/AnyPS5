#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMouseDisconnectAll();
int APS5_VABI sceMouseExtensionGetCursor();
int APS5_VABI sceMouseExtensionRead();
int APS5_VABI sceMouseExtensionReadState();
int APS5_VABI sceMouseExtensionScalePosition(std::uint16_t* position, std::uint16_t scale);
int APS5_VABI sceMouseExtensionSetConfig();
int APS5_VABI sceMouseExtensionSetCursor();
int APS5_VABI sceMouseGetHandle();
int APS5_VABI sceMouseVirtualDeviceAddDevice();
int APS5_VABI sceMouseVirtualDeviceDeleteDevice();
int APS5_VABI sceMouseVirtualDeviceGetCursor();

}
