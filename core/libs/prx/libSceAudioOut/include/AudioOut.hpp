#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioOutInit();
int APS5_VABI sceAudioOutOpen(int userId, int type, int index, std::uint32_t len, std::uint32_t freq, std::uint32_t param);
int APS5_VABI sceAudioOutClose(int handle);
int APS5_VABI sceAudioOutOutput(int handle, const void* ptr);
int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num);
int APS5_VABI sceAudioOutSetVolume(int handle, std::uint32_t flag, int* vol);
int APS5_VABI sceAudioOutGetLastOutputTime(int handle, std::uint64_t* outputTime);
int APS5_VABI sceAudioOutGetPortState(int handle, AudioOutPortState* state);
int APS5_VABI sceAudioOutDeviceIdOpen(std::uint64_t deviceId, std::int32_t type, std::int32_t index, std::uint32_t length, std::int32_t frequency, std::uint32_t parameter);
int APS5_VABI sceAudioOutSysClose(int handle);
int APS5_VABI sceAudioOutSysConfigureOutput(int type, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option);
int APS5_VABI sceAudioOutSysGetHdmiMonitorInfo(int type, void* info, std::uint32_t size);
int APS5_VABI sceAudioOutSysOpen(int userId, std::uint32_t mode);
int APS5_VABI sceAudioOutSetMixLevelPadSpk(int handle, std::uint32_t mixLevel);
int APS5_VABI sceAudioOutExClose(int handle);
int APS5_VABI sceAudioOutExConfigureOutput(int control, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option);
int APS5_VABI sceAudioOutExGetMonitorInfo(int type, int index, void* info, std::uint32_t size);
int APS5_VABI sceAudioOutExOpen(int userId, std::uint32_t mode);
int APS5_VABI sceAudioOutGetSystemState(AudioOutSystemState* state);
int APS5_VABI sceAudioOutOpenEx(void);
int APS5_VABI sceAudioOutSetSystemDebugState(void);
int APS5_VABI sceAudioOutGetHandleStatusInfo(void);
int APS5_VABI sceAudioOutGetSimulatedHandleStatusInfo2(void);
int APS5_VABI sceAudioOutChangeAppModuleState(void);
int APS5_VABI sceAudioOutGetSimulatedHandleStatusInfo(void);
int APS5_VABI sceAudioOutGetSimulatedBusUsableStatusByBusType(void);
int APS5_VABI sceAudioOutAttachToApplicationByPid(void);
int APS5_VABI sceAudioOutDetachFromApplicationByPid(void);
int APS5_VABI sceAudioOutPtOpen(void);
int APS5_VABI sceAudioOutPtClose(void);
int APS5_VABI sceAudioOutPtGetLastOutputTime(void);
int APS5_VABI sceAudioOutExPtOpen(void);
int APS5_VABI sceAudioOutExPtClose(void);
int APS5_VABI sceAudioOutExPtGetLastOutputTime(void);
int APS5_VABI sceAudioOutExGetLastOutputTime(void);
int APS5_VABI sceAudioOutGetInfoOpenNum(void);
int APS5_VABI sceAudioOutGetInfo(void);
int APS5_VABI sceAudioOutMasteringInit(uint32_t flags);
int APS5_VABI sceAudioOutMasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t flags);
int APS5_VABI sceAudioOutMasteringGetState(AudioOut2MasteringStatesHeader* state);
int APS5_VABI sceAudioOutMasteringTerm(void);
int APS5_VABI sceAudioOutSetPadSpkVolume(void);
int APS5_VABI sceAudioOutSetPadJackVolume(void);
int APS5_VABI sceAudioOutSetUsbVolume(void);
int APS5_VABI sceAudioOutSetCaesarVolume(void);
int APS5_VABI sceAudioOutSetHeadphoneOutMode(void);
int APS5_VABI sceAudioOutGetFocusEnablePid(void);
int APS5_VABI sceAudioOutSetMainOutput(void);
int APS5_VABI sceAudioOutSetVolumeDown(void);
int APS5_VABI sceAudioOutSetChatBalance(void);
int APS5_VABI sceAudioOutSetRecMode(void);
int APS5_VABI sceAudioOutSetAllMute(void);
int APS5_VABI sceAudioOutSetSparkEQ(void);
int APS5_VABI sceAudioOutArbitrationInitialize(void);
int APS5_VABI sceAudioOutEnable3DAudioByArbitration(void);
int APS5_VABI sceAudioOutAssignHRTF(void);
int APS5_VABI sceAudioOutEnable3DAudioForUser(void);
int APS5_VABI sceAudioOutEnableMonoHpOutput(void);
int APS5_VABI sceAudioOutEnableTVVirtual(void);
int APS5_VABI sceAudioOutEnableTVCorrection(void);
int APS5_VABI sceAudioOutEnableVrControllerLowPowerMode(void);
int APS5_VABI sceAudioOutEnableCinematicVrRotation(void);
int APS5_VABI sceAudioOutCustomHrtfIsExist(void);
int APS5_VABI sceAudioOutExConfigureOutputMode(void);
int APS5_VABI sceAudioOutExSystemInfoIsSupportedAudioOutExMode(void);
int APS5_VABI sceAudioOutExGetSystemInfo(void);
int APS5_VABI sceAudioOutExGetOutputInfo(void);
int APS5_VABI sceAudioOutSysConfigureOutputMode(void);
int APS5_VABI sceAudioOutSysGetMonitorInfo(void);
int APS5_VABI sceAudioOutSysHdmiMonitorInfoIsSupportedAudioOutMode(void);
int APS5_VABI sceAudioOutSysGetSystemInfo(void);
int APS5_VABI sceAudioOutSysGetOutputInfo(void);

}
