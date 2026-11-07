#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceHidControlConnectHandle(std::uint32_t first, std::uint32_t second, std::uint32_t third);
int APS5_VABI sceHidControlConnectPort(std::uint32_t first, std::uint32_t second);
int APS5_VABI sceHidControlConnectPswDeviceForNearby(std::uint32_t device, std::uint32_t value);
int APS5_VABI sceHidControlDisconnectDevice(std::uint32_t device);
int APS5_VABI sceHidControlDisconnectHandle(std::uint32_t first, std::uint32_t second);
int APS5_VABI sceHidControlDisconnectPort(std::uint32_t first, std::uint32_t second);
int APS5_VABI sceHidControlEnableMicBeamForming(std::uint32_t device, std::uint8_t enable);
int APS5_VABI sceHidControlGetAuthInformation(std::uint16_t first, std::uint16_t second, std::uint32_t kind, void* output);
int APS5_VABI sceHidControlGetBatteryState(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetDeviceId(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetDeviceInfo(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetDeviceName(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetJediExtensionUnitInfo(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetJediState(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswAllEqBandValues(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswBatteryState(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswCommand(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(std::uint8_t first, std::uint8_t second, void* output);
int APS5_VABI sceHidControlGetPswDeviceInfo(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswDeviceName(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswDeviceNameForNearby(std::uint32_t device, std::uint32_t value, void* output);
int APS5_VABI sceHidControlGetPswEqBandValues(std::uint32_t device, std::uint32_t preset, void* output);
int APS5_VABI sceHidControlGetPswEqPresetName(std::uint32_t device, std::uint32_t preset, void* output);
int APS5_VABI sceHidControlGetPswEqPresetSelection(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswNetworkLocale(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswSidetoneLevel(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetPswState(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetSparkState(std::uint32_t device, void* output);
int APS5_VABI sceHidControlGetVersionInfo(std::uint32_t device, void* output);
int APS5_VABI sceHidControlInit();
int APS5_VABI sceHidControlIsForceUpdateNeeded(std::uint32_t device, std::uint32_t value, std::uint8_t* needed);
int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(std::uint32_t device, void* output);
int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(std::uint32_t* status);
int APS5_VABI sceHidControlPswUpdateGetStatus(std::uint32_t device, std::uint32_t* status);
int APS5_VABI sceHidControlPswUpdateIsReady(std::uint32_t device, std::uint32_t* state);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(std::uint32_t device, const void* image, std::uint16_t* result);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(std::uint32_t device, const std::uint16_t* version, std::uint16_t* first, std::uint16_t* second);
int APS5_VABI sceHidControlPswUpdateSendAppendixData(std::uint32_t device, const void* chunk);
int APS5_VABI sceHidControlPswUpdateSendData(std::uint32_t device, const void* chunk);
int APS5_VABI sceHidControlPswUpdateStart(std::uint32_t device, const void* image);
int APS5_VABI sceHidControlSetAllVibrationMute(std::uint8_t mute);
int APS5_VABI sceHidControlSetApplicationFocus(std::uint32_t first, const void* data);
int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess(std::uint32_t process);
int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(std::uint8_t enable);
int APS5_VABI sceHidControlSetHandleState(std::uint32_t first, std::uint32_t second, std::uint32_t third);
int APS5_VABI sceHidControlSetJediAudioOutPath(std::uint32_t device, std::uint32_t path);
int APS5_VABI sceHidControlSetJediVolumeGain(std::uint32_t device, const void* gain);
int APS5_VABI sceHidControlSetProcessFocus(std::uint32_t first, const void* data);
int APS5_VABI sceHidControlSetPswCommand(std::uint32_t device, const void* command);
int APS5_VABI sceHidControlSetPswConsoleMute(std::uint32_t device, std::uint8_t mute);
int APS5_VABI sceHidControlSetPswDeviceName(std::uint32_t device, const void* name);
int APS5_VABI sceHidControlSetPswEqBandValues(std::uint32_t device, std::uint32_t preset, const void* values);
int APS5_VABI sceHidControlSetPswEqPresetName(std::uint32_t device, std::uint32_t preset, const void* name);
int APS5_VABI sceHidControlSetPswEqPresetSelection(std::uint32_t device, std::uint32_t preset);
int APS5_VABI sceHidControlSetPswNetworkLocale(std::uint32_t device, std::uint32_t locale);
int APS5_VABI sceHidControlSetPswResetSettings(std::uint32_t device);
int APS5_VABI sceHidControlSetPswSidetoneLevel(std::uint32_t device, std::uint8_t level);
int APS5_VABI sceHidControlSetPswSpeakerVolume(std::uint32_t device, std::uint8_t volume);
int APS5_VABI sceHidControlSetRemoteMergeProcess(std::uint32_t first, std::uint32_t second);
int APS5_VABI sceHidControlSetRevokeList(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlSetRevokeList2(std::uint32_t first, std::uint32_t count, const void* list);
int APS5_VABI sceHidControlSetUserColor(std::uint32_t device, std::uint32_t color);
int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(std::uint32_t device, std::uint8_t value);

}
