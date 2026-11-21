extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAvSetting/include/InventoryExports.hpp"
#include "prx/libSceAvSetting/include/Api.hpp"
#include <algorithm>
#include <cerrno>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

template <typename TValue>
bool ReadAvSettingField(const void* object, std::size_t offset, TValue& value) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address == 0 || offset > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    const int savedErrno = errno;
    try {
        const bool copied = GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
        errno = savedErrno;
        return copied;
    } catch (...) {
        errno = savedErrno;
        return false;
    }
}

void CopyOutPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
}

}

namespace {

constexpr int Error0001 = static_cast<int>(0x802A0001u);
constexpr int Error0002 = static_cast<int>(0x802A0002u);
constexpr int Error0007 = static_cast<int>(0x802A0007u);
constexpr int ErrorNotInitialized = static_cast<int>(0x802A001Au);

bool ProcessAttributeAccepted(std::uint32_t attribute, std::uint32_t flag) {
    const bool known = attribute == 0x20 || attribute == 0x80 || attribute == 0x4000 || attribute == 0x2000000;
    return known && flag <= 1;
}

bool ReadWord(const void* source, std::uint32_t& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    return copied;
}

} // namespace


int APS5_VABI sceAvControlGetPuTypeByMonitorInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlSetDitherModeForDiag(std::uint32_t mode) {
    if (mode > 3) return Error0002;
    return ErrorNotInitialized;
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

int APS5_VABI sceAvControlGetRawMonitorInfo(std::uint64_t argument1, void* info) {
    if (argument1 != 0x7000) return Error0007;
    if (info == nullptr) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetKsvList(std::uint64_t, void* list) {
    if (list == nullptr) return Error0001;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetRawEdid(std::uint64_t argument1, void* edid) {
    if (argument1 != 0x7000) return Error0007;
    if (edid == nullptr) return Error0002;
    return ErrorNotInitialized;
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

int APS5_VABI sceAvControlChangeProcessAttribute(std::uint32_t attribute, std::uint32_t flag) {
    if (!ProcessAttributeAccepted(attribute, flag)) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlChangeScanInMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentDeviceId(std::uint32_t argument1, void* id) {
    if (argument1 < 1 || argument1 > 3) return Error0002;
    if (id == nullptr) return Error0001;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetCurrentOutputMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetCurrentOutputModeWithSync() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvControlGetDetailHdcpStatus(std::uint32_t argument1, void* status) {
    if (status == nullptr || argument1 < 1 || argument1 > 3) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetDeviceInfo(std::uint64_t, void* info) {
    if (info == nullptr) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetHdcpStatus(std::uint32_t argument1, void* status) {
    if (status == nullptr || argument1 != 1) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetMonitorInfo(std::uint64_t, void* info) {
    if (info == nullptr) return Error0001;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetNativeMonitorInfo(std::uint64_t, void* info) {
    if (info == nullptr) return Error0001;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetPuStatus(void* status) {
    if (status == nullptr) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlGetPuType(void* type) {
    if (type == nullptr) return Error0002;
    return ErrorNotInitialized;
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

int APS5_VABI sceAvControlIsDeviceConnected(std::int32_t device) {
    if (device != 1) return Error0002;
    return ErrorNotInitialized;
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

int APS5_VABI sceAvControlSet2dVrMode(std::uint32_t mode) {
    if (mode > 1) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetAudioMuteForShutdown(std::uint32_t mute) {
    if (mute > 1) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetAvOutputMode(std::uint32_t mode, const void* argument2) {
    if (mode >= 2 || argument2 != nullptr) return Error0002;
    return ErrorNotInitialized;
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
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetGamutMetadata(std::uint64_t argument1) {
    if (argument1 != 0x7000) return Error0007;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetHdcpEncryption(std::uint64_t argument1, std::uint32_t enable) {
    if (argument1 != 0x7000) return Error0007;
    if (enable > 1) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetHdcpStatus(std::uint32_t argument1, std::uint32_t argument2) {
    if (argument1 != 1 || argument2 >= 2) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetHdmiGamutMetadata() {
    return ErrorNotInitialized;
}

int APS5_VABI sceAvControlSetHdrMetadata(std::uint64_t type) {
    if (type != 0x7000) return Error0007;
    return ErrorNotInitialized;
}

void APS5_VABI sceAvControlSetVideoOutSource(std::int32_t* out, std::int32_t source) {
    if (out != nullptr) {
        CopyOutPreservingErrno(out, &source, sizeof(source));
    }
}

int APS5_VABI sceAvControlSimulateProcessOutputModeArbitration() {
    return ErrorNotInitialized;
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

namespace {

void InitializeCallbackFuncs_nid_no_patch(void* callbacks, std::uint32_t size) {
    if (callbacks == nullptr) return;
    std::vector<std::uint8_t> buffer(size, 0);
    for (std::uint32_t i = 0; i < sizeof(size) && i < size; ++i) {
        buffer[i] = static_cast<std::uint8_t>(size >> (8 * i));
    }
    CopyOutPreservingErrno(callbacks, buffer.data(), size);
}

}

void APS5_VABI sceAvSettingCallbackFuncsInit(void* callbacks) {
    InitializeCallbackFuncs_nid_no_patch(callbacks, 0x20);
}

void APS5_VABI sceAvSettingCallbackFuncsInitForLnc(void* callbacks) {
    InitializeCallbackFuncs_nid_no_patch(callbacks, 0x10);
}

void APS5_VABI sceAvSettingCallbackFuncsInitForLnc_(void* callbacks) {
    InitializeCallbackFuncs_nid_no_patch(callbacks, 0x10);
}

void APS5_VABI sceAvSettingCallbackFuncsInit_(void* callbacks) {
    InitializeCallbackFuncs_nid_no_patch(callbacks, 0x20);
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

int APS5_VABI sceAvSettingChangeProcessAttribute(std::uint32_t attribute, std::uint32_t flag) {
    if (!ProcessAttributeAccepted(attribute, flag)) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvSettingCheckCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingControlHdcpEncryption(std::uint64_t, std::uint64_t, std::uint32_t enable) {
    if (enable > 1) return Error0002;
    return ErrorNotInitialized;
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

int APS5_VABI sceAvSettingDebugSetProcessAttribute(std::uint32_t, std::uint32_t argument2) {
    if (argument2 > 0x0FFFFFFF) return Error0002;
    return ErrorNotInitialized;
}

int APS5_VABI sceAvSettingDriverChangeConnectionStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingDriverUpdateStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvSettingEnterAudioMuteForShutdown(std::uint32_t mute) {
    if (mute > 1) return Error0002;
    return ErrorNotInitialized;
}

}
