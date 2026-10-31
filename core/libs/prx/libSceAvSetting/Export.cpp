#include "prx/libSceAvSetting/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

template <typename TValue>
bool ReadAvSettingField(const void* object, std::size_t offset, TValue& value) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    try {
        return GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
    } catch (...) {
        return false;
    }
}

}

extern "C" {

int APS5_VABI sceAvControlGetPuTypeByMonitorInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetDitherModeForDiag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetLayoutModeForDiag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetMonitorFakeMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentOutputSysMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetNativeHdmiMonitorInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetRawMonitorInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetKsvList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetRawEdid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlRemoveProcessOutputMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlUpdateDriverStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlNotifyUmdEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlAddCallbacksForLnc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlAddCallbacksForResourceArbitrator(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlRemoveCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlCheckCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlWaitCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetColorMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlCheckMonitorInfoCapability(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlCheckVideoOutMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetCheckoutputMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetSystemVideoMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlDebugAddCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlInit2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAvControlChangeOutputLayout() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlChangeOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlChangeOutputModeForDiag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlChangeProcessAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlChangeScanInMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentOutputModeWithSync() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetDetailHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetNativeMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetPuStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetPuType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlIsAudioOutModeSupportedByMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceAvControlIsColorimetryHdr(std::uint32_t colorimetry) {
    constexpr std::array<std::uint32_t, 12> isHdr{1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1};
    return colorimetry >= 9 && colorimetry <= 20 && isHdr[colorimetry - 9] != 0;
}

bool APS5_VABI sceAvControlIsDeepColorSupportedByMonitorInfo(const void* monitorInfo) {
    std::uint8_t deepColorFlags = 0;
    if (!ReadAvSettingField(monitorInfo, 0x74, deepColorFlags)) return false;
    if ((deepColorFlags & 0x0a) != 0) return true;
    std::uint8_t deepColorFlags2 = 0;
    if (!ReadAvSettingField(monitorInfo, 0x75, deepColorFlags2)) return false;
    if ((deepColorFlags2 & 0x0a) != 0) return true;
    std::uint8_t hdrFlags = 0;
    return ReadAvSettingField(monitorInfo, 0x352, hdrFlags) && (hdrFlags & 0x03) != 0;
}

int APS5_VABI sceAvControlIsDeviceConnected() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceAvControlIsHdrSupportedByMonitorInfo(const void* monitorInfo) {
    std::uint8_t capabilityFlags = 0;
    if (!ReadAvSettingField(monitorInfo, 0x358, capabilityFlags) || (capabilityFlags & 0x04) == 0)
        return false;
    std::uint8_t hdrFlags = 0;
    return ReadAvSettingField(monitorInfo, 0x78, hdrFlags) && (hdrFlags & 0x30) != 0;
}

bool APS5_VABI sceAvControlIsModeGen1Vr(const void* videoOutMode) {
    std::uint32_t modeFlags = 0;
    return ReadAvSettingField(videoOutMode, 0x30, modeFlags) && (modeFlags & 0x02) != 0;
}

bool APS5_VABI sceAvControlIsModeSystemDefault(const void* videoOutMode) {
    std::uint32_t modeFlags = 0;
    return ReadAvSettingField(videoOutMode, 0x30, modeFlags) && (modeFlags & 0x01) == 0;
}

int APS5_VABI sceAvControlIsSuspendedProcessOutputModeAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlIsVideoOutModeSupportedByMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSet2dVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetAudioMuteForShutdown() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetAvOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetColorEffect() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetDefaultAudioOutMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetDefaultVideoOutMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetDispclk() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetGamutMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetHdcpEncryption() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetHdmiGamutMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetHdrMetadata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetVideoOutSource() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSimulateProcessOutputModeArbitration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingAddCallbacks() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingAddCallbacksForLnc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingAddCallbacksForSocialScreen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingCallbackFuncsInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingCallbackFuncsInitForLnc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingCallbackFuncsInitForLnc_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingCallbackFuncsInit_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingChangeOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingChangeOutputMode2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingChangeOutputMode3() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingChangeOutputModeForDiag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingChangeProcessAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingCheckCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingControlHdcpEncryption() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDebugAddCallbacks() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDebugClearDiagCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDebugSetDiagState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDebugSetHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDebugSetProcessAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDriverChangeConnectionStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDriverUpdateStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingEnterAudioMuteForShutdown() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetCurrentDeviceInfo_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetCurrentHdmiDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetCurrentOutputMode2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetCurrentOutputMode_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetDetailedHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetHdmiConnectDisconnectNum() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetHdmiKsvList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetHdmiRawEdid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetMorpheusPuStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetNativeHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingGetRawHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsSupportedAudioOutModeByHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsSupportedHdcpVersionByHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsSupportedVideoOutModeByHdmiMonitorInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsSuspendedProcessOutputModeAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsSuspendedProcessOutputModeAvailable2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingIsVrSupportedByHdmiMonitorInfo2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingNotifyAudioOutMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingNotifyResponseForSocialScreen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingNotifyUmdEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingNotifyVideoOutMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingRemoveCallbacks() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingRemoveProcessOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSet2dVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetAudioOutModeAny_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetAudioOutModeInvalid_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetAvOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetDispclk() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetDisplayBlackLevel() {
    return static_cast<int>(0x802A00FF);
}

int APS5_VABI sceAvSettingSetHdcpMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetHdcpStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetHdmiGamutMetadata_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetVideoOutModeAny_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetVideoOutModeInvalid_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetVideoOutSource() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSetVideoOutputColorEffect() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingSimulateProcessOutputModeArbitration_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingTerm() {
    return static_cast<int>(0x802A00FF);
}

int APS5_VABI sceAvSettingWaitCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
