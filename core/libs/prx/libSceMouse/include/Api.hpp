#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMouseInit();
int APS5_VABI sceMouseOpen(int userId, std::int32_t type, std::int32_t index, const void* param);
int APS5_VABI sceMouseClose(std::int32_t handle);
int APS5_VABI sceMouseRead(std::int32_t handle, MouseData* data, std::int32_t num);
int APS5_VABI sceMouseResetAbsolutePosition(std::int32_t handle, std::uint16_t x, std::uint16_t y);
int APS5_VABI sceMouseConnectPort();
int APS5_VABI sceMouseDebugGetDeviceId();
int APS5_VABI sceMouseDeviceOpen();
int APS5_VABI sceMouseDisconnectDevice();
int APS5_VABI sceMouseDisconnectPort();
int APS5_VABI sceMouseGetDeviceInfo();
int APS5_VABI sceMouseMbusInit();
int APS5_VABI sceMouseSetHandType();
int APS5_VABI sceMouseSetPointerSpeed();
int APS5_VABI sceMouseSetProcessPrivilege();

}
