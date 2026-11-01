#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceHidControl/include/InventoryExports.hpp"
#include "prx/libSceHidControl/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceHidControlEnableMicBeamForming() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetAllVibrationMute(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlIsForceUpdateNeeded(std::uint32_t param_1, std::uint32_t param_2, std::uint8_t* param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetRemoteMergeProcess(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(std::uint8_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(std::uint32_t param_1, std::int64_t param_2, std::uint16_t* param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(std::uint32_t param_1, std::uint16_t* param_2, std::uint16_t* param_3, std::uint16_t* param_4) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateIsReady(std::uint32_t param_1, std::uint32_t* param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateStart(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateGetStatus(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateSendData(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateSendAppendixData(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(std::uint32_t param_1, std::uint32_t* param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswState(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswBatteryState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswDeviceName(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswDeviceName(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswConsoleMute(std::uint32_t param_1, std::uint8_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswSpeakerVolume(std::uint32_t param_1, std::uint8_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswSidetoneLevel(std::uint32_t param_1, std::uint8_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswSidetoneLevel(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswEqPresetSelection(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswEqPresetSelection(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswAllEqBandValues(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswResetSettings(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswNetworkLocale(std::uint32_t param_1, std::int32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswNetworkLocale(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswDeviceNameForNearby() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlConnectPswDeviceForNearby(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetPswCommand(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswCommand(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(std::uint32_t param_1, std::uint32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlConnectHandle(std::uint32_t param_1, std::uint32_t param_2, std::uint32_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlConnectPort(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlDisconnectDevice(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlDisconnectHandle(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlDisconnectPort(std::uint32_t param_1, std::uint32_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetAuthInformation(std::uint16_t param_1, std::uint16_t param_2, std::uint32_t param_3, std::uint8_t* param_4) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetBatteryState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetDeviceId(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetDeviceInfo(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetDeviceName() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetJediExtensionUnitInfo(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetJediState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetSparkState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlGetVersionInfo(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetApplicationFocus(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetHandleState(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetJediAudioOutPath(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetJediVolumeGain(std::int64_t param_1, std::uint32_t param_2, std::uint32_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetProcessFocus(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetRevokeList(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetRevokeList2(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetUserColor(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceHidControlGetControllerSerialNumberInformation(std::uint32_t param_1, std::uint8_t* param_2) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlGetPswNearbyDeviceInfo(std::uint32_t param_1, std::int64_t param_2) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlPswUpdateInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlPswUpdateTerm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlSetPswControllerCommunicationMethod(std::uint32_t param_1) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_00qes5NswlQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("00qes5NswlQ", libSceHidControlUnknown_00qes5NswlQ_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8fIm7UR4VDc", libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AjBZjowWaFI", libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C05jP9w4PCc", libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DUGI7AXFtV0", libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HZJRMD3BeKk", libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OFyJjudLKlA", libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TKax3WNHtEg", libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_WikCB91EU0w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WikCB91EU0w", libSceHidControlUnknown_WikCB91EU0w_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("i5ALHbvjRi8", libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iVF5VrBHJzk", libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tcDE01gUoN0", libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("w2MEBm2Tvm8", libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yJJq5Arw8EI", libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch);

}
