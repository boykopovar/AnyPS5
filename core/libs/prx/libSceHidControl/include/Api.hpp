#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceHidControlConnectHandle(std::uint32_t param_1, std::uint32_t param_2, std::uint32_t param_3);
int APS5_VABI sceHidControlConnectPort(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlConnectPswDeviceForNearby(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlDisconnectDevice(std::uint32_t param_1);
int APS5_VABI sceHidControlDisconnectHandle(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlDisconnectPort(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlEnableMicBeamForming();
int APS5_VABI sceHidControlGetAuthInformation(std::uint16_t param_1, std::uint16_t param_2, std::uint32_t param_3, std::uint8_t* param_4);
int APS5_VABI sceHidControlGetBatteryState();
int APS5_VABI sceHidControlGetDeviceId(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetDeviceInfo(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetDeviceName();
int APS5_VABI sceHidControlGetJediExtensionUnitInfo(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetJediState();
int APS5_VABI sceHidControlGetPswAllEqBandValues(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswBatteryState();
int APS5_VABI sceHidControlGetPswCommand(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(std::uint32_t param_1, std::uint32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlGetPswDeviceInfo();
int APS5_VABI sceHidControlGetPswDeviceName(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswDeviceNameForNearby();
int APS5_VABI sceHidControlGetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlGetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlGetPswEqPresetSelection(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswNetworkLocale(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswSidetoneLevel(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetPswState(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlGetSparkState();
int APS5_VABI sceHidControlGetVersionInfo(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlInit();
int APS5_VABI sceHidControlIsForceUpdateNeeded(std::uint32_t param_1, std::uint32_t param_2, std::uint8_t* param_3);
int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(std::uint32_t param_1, std::uint32_t* param_2);
int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(std::uint32_t param_1);
int APS5_VABI sceHidControlPswUpdateGetStatus(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlPswUpdateIsReady(std::uint32_t param_1, std::uint32_t* param_2);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(std::uint32_t param_1, std::int64_t param_2, std::uint16_t* param_3);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(std::uint32_t param_1, std::uint16_t* param_2, std::uint16_t* param_3, std::uint16_t* param_4);
int APS5_VABI sceHidControlPswUpdateSendAppendixData(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlPswUpdateSendData(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlPswUpdateStart(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlSetAllVibrationMute(std::uint32_t param_1);
int APS5_VABI sceHidControlSetApplicationFocus(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess();
int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(std::uint8_t param_1);
int APS5_VABI sceHidControlSetHandleState(std::uint32_t param_1);
int APS5_VABI sceHidControlSetJediAudioOutPath(std::uint32_t param_1);
int APS5_VABI sceHidControlSetJediVolumeGain(std::int64_t param_1, std::uint32_t param_2, std::uint32_t param_3);
int APS5_VABI sceHidControlSetProcessFocus(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlSetPswCommand(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlSetPswConsoleMute(std::uint32_t param_1, std::uint8_t param_2);
int APS5_VABI sceHidControlSetPswDeviceName(std::uint32_t param_1, std::int64_t param_2);
int APS5_VABI sceHidControlSetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlSetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlSetPswEqPresetSelection(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlSetPswNetworkLocale(std::uint32_t param_1, std::int32_t param_2);
int APS5_VABI sceHidControlSetPswResetSettings(std::uint32_t param_1);
int APS5_VABI sceHidControlSetPswSidetoneLevel(std::uint32_t param_1, std::uint8_t param_2);
int APS5_VABI sceHidControlSetPswSpeakerVolume(std::uint32_t param_1, std::uint8_t param_2);
int APS5_VABI sceHidControlSetRemoteMergeProcess(std::uint32_t param_1, std::uint32_t param_2);
int APS5_VABI sceHidControlSetRevokeList(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlSetRevokeList2(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3);
int APS5_VABI sceHidControlSetUserColor(std::uint32_t param_1);
int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(std::uint32_t param_1, std::uint32_t param_2);

}
