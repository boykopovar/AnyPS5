#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioInInit();
int APS5_VABI sceAudioInGetSilentState(int handle);
int APS5_VABI sceAudioInInput(int handle, void* dest);
int APS5_VABI sceAudioInOpen(int userId, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param);
int32_t APS5_VABI sceAudioInClose(int32_t handle);
int32_t APS5_VABI sceAudioInHqOpen(int32_t userId, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param);
int32_t APS5_VABI sceAudioInAsyncOpen(int32_t userId, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param);
int32_t APS5_VABI sceAudioInIsPadEmbeddedMic(int32_t handle);
int32_t APS5_VABI sceAudioInGetGain(int32_t handle, float* gain);
int32_t APS5_VABI sceAudioInGetRerouteCount(int32_t handle);
int APS5_VABI sceAudioInDeviceIdOpen(int userId, uint32_t type, uint32_t deviceId, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInDeviceIdAsyncOpen(int userId, uint32_t type, uint32_t deviceId, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInSetUsbGain(void);
int APS5_VABI sceAudioInIsSharedDevice(int handle);
int APS5_VABI sceAudioInDeviceIdHqOpen(int userId, uint32_t type, uint32_t deviceId, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInDeviceHqOpen(int userId, uint32_t deviceId, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInSetConnections(void);
int APS5_VABI sceAudioInSetPortConnections(void);
int APS5_VABI sceAudioInSetPortStatuses(void);
int APS5_VABI sceAudioInGetHandleStatusInfo(int handle, void* info);
int APS5_VABI sceAudioInOpenEx(int userId, uint32_t type, uint32_t index, uint32_t extra, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInHqOpenEx(int userId, uint32_t type, uint32_t index, uint32_t extra, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInDeviceOpenEx(int userId, uint32_t type, uint32_t deviceId, uint32_t extra, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInChangeAppModuleState(void);
int APS5_VABI sceAudioInSetSparkParam(void);
int APS5_VABI sceAudioInSetSparkSideTone(void);
int APS5_VABI sceAudioInSetCompressorPreGain(void);
int APS5_VABI sceAudioInSetNoiseGateThreshold(void);
int APS5_VABI sceAudioInSetMicLevel(int type, uint64_t device, int level, int arg4, int arg5);
int APS5_VABI sceAudioInSetMicrophoneLevel(int type, uint64_t device, int level, int arg);
int APS5_VABI sceAudioInSetDeviceMuteStatus(int type, uint64_t device, int mute);
int APS5_VABI sceAudioInSetDevConnection(void);
int APS5_VABI sceAudioInCountPorts(int16_t type, int16_t index, int16_t arg3, int32_t arg4);
int APS5_VABI sceAudioInSetUserMute(int type, uint64_t device, int mute);
int APS5_VABI sceAudioInVmicCreate(int userId, uint32_t len, uint32_t freq, uint32_t param);
int APS5_VABI sceAudioInVmicDestroy(void);
int APS5_VABI sceAudioInVmicWrite(void);
int APS5_VABI sceAudioInVmicGetStatus(void);
int APS5_VABI sceAudioInVmicSetMute(void);
int APS5_VABI sceAudioInSetMode(int mode, int value);
int APS5_VABI sceAudioInSetMode2(int mode, int value, void* arg);
int APS5_VABI sceAudioInExtCtrl(void);
int APS5_VABI sceAudioInInputs(void);

}
