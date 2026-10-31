// Copyright (C) 2026 Boy K. Povar
// SPDX-License-Identifier: MIT
//

#include "prx/libSceHidControl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "%s: %s\n", label, "failed");
    std::abort();
}

int main() {
    // The PS5 HID library success/not-ready codes from the 3.20 export
    // (0x803b3001 == success, 0x803b3002 == not ready / not initialized).
    const std::int32_t kOk = static_cast<std::int32_t>(0x803b3001u);
    const std::int32_t kNotReady = static_cast<std::int32_t>(0x803b3002u);

    // ---- Before init: every command function reports not-ready, except
    // ---- the EQ-band value getters/setters which are no-op pre-init.
    Require(sceHidControlConnectPort(1, 2) == kNotReady, "[hid] ConnectPort pre-init");
    Require(sceHidControlConnectPort(0, 0) == kNotReady, "[hid] ConnectPort null pre-init");
    Require(sceHidControlConnectHandle(1, 2, 3) == kNotReady, "[hid] ConnectHandle pre-init");
    Require(sceHidControlDisconnectDevice(1) == kNotReady, "[hid] DisconnectDevice pre-init");
    Require(sceHidControlGetDeviceName() == kNotReady, "[hid] GetDeviceName pre-init");
    Require(sceHidControlGetPswCommand(1, 1234) == kNotReady, "[hid] GetPswCommand pre-init");
    Require(sceHidControlSetPswCommand(1, 1234) == kNotReady, "[hid] SetPswCommand pre-init");
    Require(sceHidControlGetPswState(1, 1234) == kNotReady, "[hid] GetPswState pre-init");
    Require(sceHidControlEnableMicBeamForming() == kNotReady, "[hid] EnableMicBeamForming pre-init");
    Require(sceHidControlGetBatteryState() == kNotReady, "[hid] GetBatteryState pre-init");
    Require(sceHidControlGetPswAllEqBandValues(1, 1234) == kNotReady, "[hid] AllEqBand pre-init");
    Require(sceHidControlConnectPswDeviceForNearby(1, 2) == kNotReady, "[hid] NearbConnect pre-init");

    // These four EQ-band functions return success even before init.
    Require(sceHidControlGetPswEqBandValues(1, 2, 345) == kOk, "[hid] GetPswEqBandValues pre-init");
    Require(sceHidControlSetPswEqBandValues(1, 2, 345) == kOk, "[hid] SetPswEqBandValues pre-init");
    Require(sceHidControlGetPswEqPresetName(1, 2, 345) == kOk, "[hid] GetPswEqPresetName pre-init");
    Require(sceHidControlSetPswEqPresetName(1, 2, 345) == kOk, "[hid] SetPswEqPresetName pre-init");

    // ---- Init
    Require(sceHidControlInit() == kOk, "[hid] Init");

    // ---- After init: command functions succeed, param guards apply.
    Require(sceHidControlConnectPort(1, 2) == kOk, "[hid] ConnectPort post-init");
    Require(sceHidControlConnectPort(0, 0) == kOk, "[hid] ConnectPort null post-init");
    Require(sceHidControlConnectHandle(1, 2, 3) == kOk, "[hid] ConnectHandle post-init");
    Require(sceHidControlDisconnectHandle(1, 2) == kOk, "[hid] DisconnectHandle post-init");
    Require(sceHidControlDisconnectPort(1, 2) == kOk, "[hid] DisconnectPort post-init");
    Require(sceHidControlDisconnectDevice(1) == kOk, "[hid] DisconnectDevice post-init");
    Require(sceHidControlEnableMicBeamForming() == kOk, "[hid] EnableMicBeamForming post-init");
    Require(sceHidControlGetBatteryState() == kOk, "[hid] GetBatteryState post-init");
    Require(sceHidControlGetDeviceId(1, 2) == kOk, "[hid] GetDeviceId post-init");
    Require(sceHidControlGetDeviceName() == kOk, "[hid] GetDeviceName post-init");
    Require(sceHidControlGetVersionInfo(1, 2) == kOk, "[hid] GetVersionInfo post-init");
    Require(sceHidControlGetJediState() == kOk, "[hid] GetJediState post-init");
    Require(sceHidControlGetSparkState() == kOk, "[hid] GetSparkState post-init");
    Require(sceHidControlGetAuthInformation(1, 2, 3, nullptr) == kOk, "[hid] GetAuthInformation post-init");
    Require(sceHidControlGetControllerSerialNumberInformation(1, nullptr) == kOk, "[hid] GetControllerSerial post-init");
    Require(sceHidControlGetPswNearbyDeviceInfo(1, 1234) == kOk, "[hid] GetPswNearby post-init");
    Require(sceHidControlConnectPswDeviceForNearby(1, 2) == kOk, "[hid] NearbConnect dup post-init");
    Require(sceHidControlConnectPswDeviceForNearby(1, 2) == kOk, "[hid] NearbConnect post-init");
    Require(sceHidControlSetJediVolumeGain(1234, 1, 2) == kOk, "[hid] SetJediVolumeGain post-init");
    Require(sceHidControlSetJediAudioOutPath(1) == kOk, "[hid] SetJediAudioOutPath post-init");
    Require(sceHidControlSetProcessFocus(1, 2) == kOk, "[hid] SetProcessFocus post-init");
    Require(sceHidControlSetApplicationFocus(1, 2) == kOk, "[hid] SetApplicationFocus post-init");
    Require(sceHidControlSetCollaboInputSettingOverrideProcess() == kOk, "[hid] SetCollabo post-init");
    Require(sceHidControlSetForceAdvancedVibrationMode(1) == kOk, "[hid] SetForceAdvancedVib post-init");
    Require(sceHidControlGetPswBatteryState() == kOk, "[hid] GetPswBatteryState post-init");
    Require(sceHidControlSetCollaboInputSettingOverrideProcess() == kOk, "[hid] SetCollabo dup");
    Require(sceHidControlGetPswAllEqBandValues(1, 1234) == kOk, "[hid] AllEqBand post-init");

    // ---- Param-guard arms (decompiled thresholds).
    // GetPswCommand / SetPswCommand: null payload -> success.
    Require(sceHidControlGetPswCommand(1, 0) == kOk, "[hid] GetPswCommand null payload");
    Require(sceHidControlSetPswCommand(1, 0) == kOk, "[hid] SetPswCommand null payload");
    Require(sceHidControlGetPswCommand(1, 1234) == kOk, "[hid] GetPswCommand valid payload");
    Require(sceHidControlSetPswCommand(1, 1234) == kOk, "[hid] SetPswCommand valid payload");
    Require(sceHidControlGetPswDeviceName(1, 1234) == kOk, "[hid] GetPswDeviceName valid");
    Require(sceHidControlSetPswDeviceName(1, 1234) == kOk, "[hid] SetPswDeviceName valid");
    Require(sceHidControlGetPswState(1, 1234) == kOk, "[hid] GetPswState valid");
    Require(sceHidControlGetPswNearbyDeviceInfo(1, 1234) == kOk, "[hid] GetPswNearby valid");

    // GetPswEqPresetSelection: null -> success.
    Require(sceHidControlGetPswEqPresetSelection(1, 0) == kOk, "[hid] GetPswEqPresetSelection null");
    Require(sceHidControlGetPswEqPresetSelection(1, 1) == kOk, "[hid] GetPswEqPresetSelection valid");

    // GetPswNetworkLocale / GetPswSidetoneLevel: null -> success.
    Require(sceHidControlGetPswNetworkLocale(1, 0) == kOk, "[hid] GetPswNetworkLocale null");
    Require(sceHidControlGetPswSidetoneLevel(1, 0) == kOk, "[hid] GetPswSidetoneLevel null");
    Require(sceHidControlGetPswNetworkLocale(1, 1234) == kOk, "[hid] GetPswNetworkLocale valid");
    Require(sceHidControlGetPswSidetoneLevel(1, 1234) == kOk, "[hid] GetPswSidetoneLevel valid");

    // SetPswEqPresetSelection: index >= 6 -> not ready, else success.
    Require(sceHidControlSetPswEqPresetSelection(1, 6) == kNotReady, "[hid] SetPswEqPresetSelection idx 6");
    Require(sceHidControlSetPswEqPresetSelection(1, 5) == kOk, "[hid] SetPswEqPresetSelection idx 5");
    Require(sceHidControlSetPswEqPresetSelection(1, 0) == kOk, "[hid] SetPswEqPresetSelection idx 0");

    // SetPswNetworkLocale: idx <= 0x24 -> not ready, else success.
    Require(sceHidControlSetPswNetworkLocale(1, 0) == kNotReady, "[hid] SetPswNetworkLocale idx 0");
    Require(sceHidControlSetPswNetworkLocale(1, 0x24) == kNotReady, "[hid] SetPswNetworkLocale idx 0x24");
    Require(sceHidControlSetPswNetworkLocale(1, 0x25) == kOk, "[hid] SetPswNetworkLocale idx 0x25");

    // SetPswSidetoneLevel: idx >= 0x10 -> not ready, else success.
    Require(sceHidControlSetPswSidetoneLevel(1, 0x10) == kNotReady, "[hid] SetPswSidetoneLevel idx 0x10");
    Require(sceHidControlSetPswSidetoneLevel(1, 0x0f) == kOk, "[hid] SetPswSidetoneLevel idx 0x0f");

    // SetPswSpeakerVolume: idx >= 0x10 -> not ready, else success.
    Require(sceHidControlSetPswSpeakerVolume(1, 0x10) == kNotReady, "[hid] SetPswSpeakerVolume idx 0x10");
    Require(sceHidControlSetPswSpeakerVolume(1, 0x0f) == kOk, "[hid] SetPswSpeakerVolume idx 0x0f");
    Require(sceHidControlSetPswSpeakerVolume(1, 0) == kOk, "[hid] SetPswSpeakerVolume idx 0");

    // PswUpdate functions (all guarded by init).
    Require(sceHidControlPswUpdateIsReady(1, nullptr) == kOk, "[hid] PswUpdateIsReady");
    Require(sceHidControlPswUpdateGetStatus(1, 2) == kOk, "[hid] PswUpdateGetStatus");
    Require(sceHidControlPswUpdateSendData(1, 2) == kOk, "[hid] PswUpdateSendData");
    Require(sceHidControlPswUpdateSendAppendixData(1, 2) == kOk, "[hid] PswUpdateSendAppendixData");
    Require(sceHidControlPswUpdateGetReconnectStatus(1) == kOk, "[hid] PswUpdateGetReconnectStatus");
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(1, nullptr) == kOk, "[hid] PswUpdateGetFirmwareTelemetry");
    Require(sceHidControlPswUpdateIsUpdateNeeded(1, 1234, nullptr) == kOk, "[hid] PswUpdateIsUpdateNeeded");
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(1, nullptr, nullptr, nullptr) == kOk, "[hid] PswUpdateIsUpdateNeededMulti");
    Require(sceHidControlPswUpdateStart(1, 2) == kOk, "[hid] PswUpdateStart");
    Require(sceHidControlPswUpdateInit() == kOk, "[hid] PswUpdateInit");
    Require(sceHidControlPswUpdateTerm() == kOk, "[hid] PswUpdateTerm");

    // Misc command functions (post-init).
    Require(sceHidControlSetRevokeList(1, 2, 345) == kOk, "[hid] SetRevokeList");
    Require(sceHidControlSetRevokeList2(1, 2, 345) == kOk, "[hid] SetRevokeList2");
    Require(sceHidControlSetUserColor(1) == kOk, "[hid] SetUserColor");
    Require(sceHidControlSetPswConsoleMute(1, 2) == kOk, "[hid] SetPswConsoleMute");
    Require(sceHidControlSetPswControllerCommunicationMethod(1) == kOk, "[hid] SetPswControllerCommunicationMethod");
    Require(sceHidControlSetRemoteMergeProcess(1, 2) == kOk, "[hid] SetRemoteMergeProcess");
    Require(sceHidControlSetVibrationTriggerEffectWeakForce(1, 2) == kOk, "[hid] SetVibrationTriggerEffectWeakForce");
    Require(sceHidControlSetHandleState(1) == kOk, "[hid] SetHandleState");
    Require(sceHidControlSetPswResetSettings(1) == kOk, "[hid] SetPswResetSettings");

    return 0;
}
