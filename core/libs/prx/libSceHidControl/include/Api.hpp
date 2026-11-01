#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceHidControlEnableMicBeamForming(void);
int APS5_VABI sceHidControlSetAllVibrationMute(void);
int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(void);
int APS5_VABI sceHidControlIsForceUpdateNeeded(void);
int APS5_VABI sceHidControlSetRemoteMergeProcess(void);
int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess(void);
int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(void);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(void);
int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(void);
int APS5_VABI sceHidControlPswUpdateIsReady(void);
int APS5_VABI sceHidControlPswUpdateStart(void);
int APS5_VABI sceHidControlPswUpdateGetStatus(void);
int APS5_VABI sceHidControlPswUpdateSendData(void);
int APS5_VABI sceHidControlPswUpdateSendAppendixData(void);
int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(void);
int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(void);
int APS5_VABI sceHidControlGetPswState(void);
int APS5_VABI sceHidControlGetPswBatteryState(void);
int APS5_VABI sceHidControlSetPswDeviceName(void);
int APS5_VABI sceHidControlGetPswDeviceName(void);
int APS5_VABI sceHidControlSetPswConsoleMute(void);
int APS5_VABI sceHidControlSetPswSpeakerVolume(void);
int APS5_VABI sceHidControlSetPswSidetoneLevel(void);
int APS5_VABI sceHidControlGetPswSidetoneLevel(void);
int APS5_VABI sceHidControlSetPswEqPresetSelection(void);
int APS5_VABI sceHidControlGetPswEqPresetSelection(void);
int APS5_VABI sceHidControlSetPswEqPresetName(void);
int APS5_VABI sceHidControlGetPswEqPresetName(void);
int APS5_VABI sceHidControlSetPswEqBandValues(void);
int APS5_VABI sceHidControlGetPswEqBandValues(void);
int APS5_VABI sceHidControlGetPswAllEqBandValues(void);
int APS5_VABI sceHidControlSetPswResetSettings(void);
int APS5_VABI sceHidControlSetPswNetworkLocale(void);
int APS5_VABI sceHidControlGetPswNetworkLocale(void);
int APS5_VABI sceHidControlGetPswDeviceNameForNearby(void);
int APS5_VABI sceHidControlConnectPswDeviceForNearby(void);
int APS5_VABI sceHidControlGetPswDeviceInfo(void);
int APS5_VABI sceHidControlSetPswCommand(void);
int APS5_VABI sceHidControlGetPswCommand(void);
int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(void);
int APS5_VABI sceHidControlConnectHandle(void);
int APS5_VABI sceHidControlConnectPort(void);
int APS5_VABI sceHidControlDisconnectDevice(void);
int APS5_VABI sceHidControlDisconnectHandle(void);
int APS5_VABI sceHidControlDisconnectPort(void);
int APS5_VABI sceHidControlGetAuthInformation(void);
int APS5_VABI sceHidControlGetBatteryState(void);
int APS5_VABI sceHidControlGetDeviceId(void);
int APS5_VABI sceHidControlGetDeviceInfo(void);
int APS5_VABI sceHidControlGetDeviceName(void);
int APS5_VABI sceHidControlGetJediExtensionUnitInfo(void);
int APS5_VABI sceHidControlGetJediState(void);
int APS5_VABI sceHidControlGetSparkState(void);
int APS5_VABI sceHidControlGetVersionInfo(void);
int APS5_VABI sceHidControlInit(void);
int APS5_VABI sceHidControlSetApplicationFocus(void);
int APS5_VABI sceHidControlSetHandleState(void);
int APS5_VABI sceHidControlSetJediAudioOutPath(void);
int APS5_VABI sceHidControlSetJediVolumeGain(void);
int APS5_VABI sceHidControlSetProcessFocus(void);
int APS5_VABI sceHidControlSetRevokeList(void);
int APS5_VABI sceHidControlSetRevokeList2(void);
int APS5_VABI sceHidControlSetUserColor(void);

}
