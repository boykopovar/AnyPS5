#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMoveInit(void);
int APS5_VABI sceMoveTerm(void);
int APS5_VABI sceMoveOpen(int32_t userId, int32_t type, int32_t index);
int APS5_VABI sceMoveClose(int32_t handle);
int APS5_VABI sceMoveGetDeviceInfo(int32_t handle, void* info);
int APS5_VABI sceMoveGetExtensionPortInfo(int32_t handle, void* data);
int APS5_VABI sceMoveReadStateLatest(int32_t handle, void* data);
int APS5_VABI sceMoveReadStateRecent(int32_t handle, int64_t timestamp, void* data, int32_t* outCount);
int APS5_VABI sceMoveSetVibration(int32_t handle, uint8_t motor);
int APS5_VABI sceMoveSetLightSphere(int32_t handle, uint8_t red, uint8_t green, uint8_t blue);
int APS5_VABI sceMoveResetLightSphere(int32_t handle);
int APS5_VABI sceMoveGetBluetoothAddress(void);
int APS5_VABI sceMoveGetCalibrationInfo(void);
int APS5_VABI sceMoveGetDeviceId(void);
int APS5_VABI sceMoveIsReproductionModel(void);
int APS5_VABI sceMoveIsValidHandle(void);
int APS5_VABI sceMoveReadStateRecentForTracker(void);
int APS5_VABI sceMoveSetCalibrationMode(void);
int APS5_VABI sceMoveSetExtensionPortOutput(void);
int APS5_VABI sceMoveSetLightSphereForTracker(void);
int APS5_VABI sceMoveWriteCalibrationInfo(void);

}
