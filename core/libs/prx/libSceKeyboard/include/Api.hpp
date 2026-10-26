#pragma once

#include "SceTypes.hpp"

struct KeyboardDeviceInfo;

struct KeyboardProcessFocus;

extern "C" {

int APS5_VABI sceKeyboardInit(void);
int APS5_VABI sceKeyboardOpen(int userId, int32_t type, int32_t index, const void* param);
int APS5_VABI sceKeyboardClose(int32_t handle);
int APS5_VABI sceKeyboardRead(int32_t handle, KeyboardData* data, int32_t num);
int APS5_VABI sceKeyboardReadState(int32_t handle, KeyboardData* data);
int APS5_VABI sceKeyboardGetConnection(std::int32_t handle, bool* connected);
int APS5_VABI sceKeyboardGetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint8_t led, std::uint8_t modifierKey, std::uint16_t keyCode, KeyboardCharData* charData);
int APS5_VABI sceKeyboardConnectPort(std::int32_t first, std::int32_t second);
int APS5_VABI sceKeyboardDebugGetDeviceId(std::int32_t handle, std::int32_t* deviceId);
int APS5_VABI sceKeyboardDeviceOpen(std::uint64_t deviceId, std::int32_t index, std::uint32_t options);
int APS5_VABI sceKeyboardDisconnectAll(void);
int APS5_VABI sceKeyboardDisconnectDevice(std::int32_t deviceId);
int APS5_VABI sceKeyboardDisconnectPort(std::int32_t first, std::int32_t second);
int APS5_VABI sceKeyboardGetDeviceInfo(std::int32_t deviceId, KeyboardDeviceInfo* info);
int APS5_VABI sceKeyboardGetHandle(std::int32_t userId, std::int32_t type, std::int32_t index);
int APS5_VABI sceKeyboardMbusInit(void);
int APS5_VABI sceKeyboardPadEmulateClose(void);
int APS5_VABI sceKeyboardPadEmulateOpen(void);
int APS5_VABI sceKeyboardSetProcessFocus(const KeyboardProcessFocus* focus);
int APS5_VABI sceKeyboardSetProcessPrivilege(std::uint8_t privilege);
int APS5_VABI sceDbgKeyboardClose();
int APS5_VABI sceDbgKeyboardGetKey2Char();
int APS5_VABI sceDbgKeyboardInit();
int APS5_VABI sceDbgKeyboardOpen();
int APS5_VABI sceDbgKeyboardRead();
int APS5_VABI sceDbgKeyboardReadState();

}
