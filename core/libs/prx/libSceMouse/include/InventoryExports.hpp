#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceMouseDisconnectAll(void);
int APS5_VABI sceMouseVirtualDeviceAddDevice(const void* param, void* out);
int APS5_VABI sceMouseVirtualDeviceDeleteDevice(std::int32_t id);
int APS5_VABI sceMouseVirtualDeviceGetCursor(std::int32_t id, void* cursor);

}
