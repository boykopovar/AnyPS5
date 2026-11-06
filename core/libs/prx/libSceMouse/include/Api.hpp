#pragma once

#include "SceTypes.hpp"
#include "prx/libSceMouse/include/mouse_structs.h"

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
int APS5_VABI sceMouseGetDeviceInfo(std::int32_t device, void* info);
int APS5_VABI sceMouseMbusInit();
int APS5_VABI sceMouseSetHandType(std::int32_t handle, std::uint32_t type);
int APS5_VABI sceMouseSetPointerSpeed(std::int32_t handle, std::uint32_t speed);
int APS5_VABI sceMouseSetProcessPrivilege();
int APS5_VABI sceMouseReadAbsolutePosition(std::int32_t handle, MouseAbsoluteData* data, std::int32_t num);
int APS5_VABI sceMouseSetAbsoluteHighPrecisionMode(std::int32_t handle, std::int32_t mode);
int APS5_VABI sceMouseSetAbsoluteVirtualScreenSize(std::int32_t handle, std::int32_t size);
int APS5_VABI sceMouseGetHandle(int userId, std::int32_t type, std::int32_t index);
int APS5_VABI sceMouseExtensionRead(std::int32_t handle, void* data, std::uint32_t size, std::int32_t num);
int APS5_VABI sceMouseExtensionReadState(std::int32_t handle, void* state, std::uint32_t size);
int APS5_VABI sceMouseExtensionSetConfig(std::int32_t handle, const void* config);
int APS5_VABI sceMouseExtensionSetCursor(std::int32_t handle, std::int32_t cursor);
int APS5_VABI sceMouseExtensionGetCursor(std::int32_t handle, void* cursor);
int APS5_VABI sceMouseExtensionScalePosition(std::uint16_t* position, std::uint16_t scale);

}
