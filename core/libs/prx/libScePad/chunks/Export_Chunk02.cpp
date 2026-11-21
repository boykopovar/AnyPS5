extern "C" {
#include <cerrno>
#include <cstddef>
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <thread>

#include "SceTypes.hpp"
#include "prx//libc/include/General.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

static_assert(sizeof(PadVrControllerData) == 104);
static_assert(offsetof(PadVrControllerData, triggers) == 8);
static_assert(offsetof(PadVrControllerData, touchButtons) == 12);
static_assert(offsetof(PadVrControllerData, acceleration) == 20);
static_assert(offsetof(PadVrControllerData, angularVelocity) == 32);
static_assert(offsetof(PadVrControllerData, orientation) == 44);
static_assert(offsetof(PadVrControllerData, connected) == 64);
static_assert(offsetof(PadVrControllerData, timestamp) == 72);

static_assert(sizeof(PadVirtualTriggerInternalState) == 12);
static_assert(offsetof(PadVirtualTriggerInternalState, mode) == 8);
static_assert(offsetof(PadVirtualTriggerInternalState, flags) == 9);

static_assert(sizeof(PadLightBarBlinkingParameter) == 3);
static_assert(sizeof(PadContainerIdInformation) == 0x2004);
static_assert(sizeof(PadVolumeGain) == 4);
static_assert(offsetof(PadVolumeGain, micGain) == 3);
static_assert(sizeof(PadButtonRemappingInfo) == 16);
static_assert(offsetof(PadButtonRemappingInfo, mode) == 15);

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007);
constexpr int PAD_ERROR_DEVICE_NO_HANDLE = static_cast<int>(0x80920008);
constexpr int PAD_ERROR_ALREADY_OPENED = static_cast<int>(0x80920004);
constexpr int PAD_ERROR_UNKNOWN_USER = static_cast<int>(0x809b0081);
constexpr int PAD_ERROR_INVALID_USER_ID = static_cast<int>(0x809b0001);
constexpr int PAD_ERROR_INVALID_PORT_INDEX = static_cast<int>(0x809b0003);
constexpr int PAD_ERROR_NOT_PERMITTED = static_cast<int>(0x80920101);
constexpr int PAD_USER_ID_INVALID = -1;

std::array<std::atomic_bool, PAD_MAX_CONTROLLERS> g_opened{};
std::array<std::atomic_bool, PAD_MAX_CONTROLLERS> g_everOpened{};
std::atomic_bool g_remoteOpened{false};
std::array<std::atomic<int>, PAD_SPECIAL_HANDLE_COUNT> g_specialUsers{};
std::atomic_bool g_initialized{false};
std::atomic_bool g_particularMode{false};

bool SpecialHandle(int handle) {
    if (handle < PAD_SPECIAL_HANDLE || handle >= PAD_SPECIAL_HANDLE + PAD_SPECIAL_HANDLE_COUNT) return false;
    const int slot = handle - PAD_SPECIAL_HANDLE;
    return g_specialUsers[slot] != 0;
}

int SpecialHandleOf(int userId) {
    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        if (g_specialUsers[slot] == userId) return PAD_SPECIAL_HANDLE + slot;
    }
    return PAD_ERROR_DEVICE_NO_HANDLE;
}

int PortOf(int handle) {
    if (handle < PAD_HANDLE_BASE || handle >= PAD_HANDLE_BASE + PAD_MAX_CONTROLLERS) return -1;
    const int port = handle - PAD_HANDLE_BASE;
    if (!g_everOpened[port]) return -1;
    return port;
}

bool KnownHandle(int handle) {
    return PortOf(handle) >= 0 || SpecialHandle(handle);
}

int DeviceOf(int handle) {
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (SpecialHandle(handle)) return PAD_ERROR_DEVICE_NOT_CONNECTED;
    return Pad::IsConnected(PortOf(handle)) ? PAD_OK : PAD_ERROR_DEVICE_NOT_CONNECTED;
}

bool OpenHandle(int handle) {
    const int port = PortOf(handle);
    return (port >= 0 && g_opened[port]) || SpecialHandle(handle);
}

constexpr std::size_t PAD_LEONARDO_UNIQUE_DATA_SIZE = 0x18;
constexpr std::uint32_t PAD_HISTORY_BUFFER_SIZE = 0x960;
constexpr std::int32_t PAD_OPEN_EXT2_PARAM_SIZE = 0x10;

void WriteDefaultData(PadData* data) {
    std::memset(data, 0, sizeof(*data));
    data->left_stick_x = data->left_stick_y = data->right_stick_x = data->right_stick_y = 0x80;
    data->orientation_w = 1.0f;
    data->acceleration_y = 1.0f;
}

PadData SpecialData() {
    PadData data;
    WriteDefaultData(&data);
    return data;
}

bool SystemRemote(int userId, int type) {
    return userId == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE;
}

bool RemoteHandle(int handle) {
    return g_remoteOpened && handle == PAD_REMOTE_HANDLE;
}

int OpenPortType(int handle) {
    if (SpecialHandle(handle)) return PAD_PORT_TYPE_SPECIAL;
    if (RemoteHandle(handle)) return PAD_PORT_TYPE_REMOTE;
    const int port = PortOf(handle);
    return port >= 0 && g_opened[port] ? PAD_PORT_TYPE_STANDARD : -1;
}

int PortForUser(int userId, int type, int index) {
    if (index != 0 || type != PAD_PORT_TYPE_STANDARD || userId < 0x10000000 ||
        userId >= 0x10000000 + PAD_MAX_CONTROLLERS) return -1;
    return userId - 0x10000000;
}

bool ValidPort(int userId, int type, int index) {
    if (index != 0) return false;
    if (type == PAD_PORT_TYPE_SPECIAL || SystemRemote(userId, type)) return true;
    return PortForUser(userId, type, index) >= 0;
}

}

static_assert(sizeof(PadUpdateData) == 32776);
static_assert(offsetof(PadUpdateData, data) == 8);

static_assert(sizeof(PadMotionSensorPosition) == 12);


int APS5_VABI scePadSetVibrationMode(int handle, int mode) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (mode <= 0) return PAD_ERROR_INVALID_ARG;
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (mode > 5) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetVibrationMode(PortOf(handle), mode);
 return PAD_OK;
}

int APS5_VABI scePadIsSupportedAudioFunction(int handle) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 const int device = DeviceOf(handle);
 if (device != PAD_OK) return device;
 return 1;
}

int APS5_VABI scePadSetAudioOutPath(int handle, int path) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (static_cast<unsigned>(path) > 4u) return PAD_ERROR_INVALID_ARG;
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetAudioOutPath(PortOf(handle), path);
 return PAD_OK;
}

int APS5_VABI scePadSetVolumeGain(int handle, const PadVolumeGain* gain) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (gain == nullptr) return PAD_ERROR_INVALID_ARG;
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 PadVolumeGain local{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&local, gain, sizeof(local));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 Pad::SetVolumeGain(PortOf(handle), local.speakerVolume, local.headsetVolume, local.micGain);
 return PAD_OK;
}

int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled) {
 (void)enabled;
 return PAD_OK;
}

int APS5_VABI scePadVrControllerGetDeviceInformation(std::int32_t handle, PadVrControllerDeviceInformation* info) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerRead(std::int32_t handle, PadVrControllerData* data, std::int32_t count) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerGetTriggerEffectState(std::int32_t leftHandle, std::int32_t rightHandle, PadTriggerEffectStateInformation* states) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerReadState(int handle, PadVrControllerData* state) {
 (void)handle;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerSetTriggerEffect(std::int32_t handle, const PadVrTriggerEffect* effect) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadVrControllerSetTriggerEffects(std::int32_t leftHandle, std::int32_t rightHandle, const PadVrTriggerEffects* effects) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadIsRemoteController(int handle, bool* remote) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (remote == nullptr) return PAD_ERROR_INVALID_ARG;
 const bool isRemote = false;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(remote, &isRemote, sizeof(isRemote));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return 0;
}

int APS5_VABI scePadSetAngularVelocityBiasCorrectionState(int handle, bool enabled) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (enabled) NotImplemented_nid_no_patch(__func__);
 return PAD_OK;
}

APS5_EXPORT("n3kSX62fgNo", scePadUnknown_n3kSX62fgNo);
int APS5_VABI scePadUnknown_n3kSX62fgNo() {
    return g_initialized.load(std::memory_order_acquire) ? PAD_OK : PAD_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePadVrControllerGetTriggerEffectStates(std::int32_t leftHandle, std::int32_t rightHandle, PadTriggerEffectStateInformation* states) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetAccessibilityControllerState(int handle, std::uint32_t* state) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (state == nullptr) return PAD_ERROR_INVALID_ARG;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    const std::uint32_t controllerState = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(state, &controllerState, sizeof(controllerState));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    return PAD_OK;
}

int APS5_VABI scePadSetTriggerEffectExt(std::int32_t handle, const PadTriggerEffectExtended* effect) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadLastLotusButtonData(std::int32_t handle, std::uint32_t parameter, PadLotusButtonData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadReadStateExt(std::int32_t handle, PadDataExtended* data) {
    if (data == nullptr) return PAD_ERROR_INVALID_ARG;
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle) && !RemoteHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadSetExtensionReport(std::int32_t handle, const PadExtensionReport* report) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetExtensionUnitInfo(std::int32_t handle, PadExtensionUnitInfo* info) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadEnableExtensionPort(bool enabled) {
    return 0;
}

int APS5_VABI scePadEnableSpecificDeviceClass(std::int32_t deviceClass) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (deviceClass != 9) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadEnableSpecificDeviceClass: the device-class flag");
    return 0;
}

int APS5_VABI scePadOpenExt2(std::int32_t userId, std::int32_t paramSize, const void* param) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (paramSize != PAD_OPEN_EXT2_PARAM_SIZE || param == nullptr) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(userId);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadDeviceOpen(std::uint64_t deviceId, std::int32_t type, std::int32_t index) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetDataInternal(std::uint8_t port, PadData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetUserColor(std::int32_t handle, std::uint32_t color) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableVibration(std::int32_t handle, bool disabled) {
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (SpecialHandle(handle)) return PAD_OK;
    if (disabled) {
        Pad::SetVibration(PortOf(handle), 0, 0);
    }
    return PAD_OK;
}

int APS5_VABI scePadSetVibrationStrength(std::int32_t handle, std::uint32_t strength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLightBarBaseBrightness(std::int32_t handle, std::uint32_t brightness) {
    (void)brightness;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    return PAD_OK;
}

int APS5_VABI scePadSetLightBarBlinking(std::int32_t handle, const PadLightBarBlinkingParameter* parameter) {
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (parameter == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI scePadGetIdleCount(std::int32_t userId) {
    if (!Pad::IsInitialized()) return 0;
    static_cast<void>(userId);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadGetControllerIdleCount(std::int32_t handle, std::uint64_t* count) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (handle == 0 || count == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadSetPowerSavingState(std::int32_t handle, std::uint8_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetAutoPowerOffCount(std::int32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableUsbConnection(bool enabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (enabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadEnableAutoDetect(bool enabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (enabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadEnablePlayPauseEmulation(bool enabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (enabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadDisableTrackingLedBatterySaver(bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (disabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadSetLoginUserNumber(std::int32_t number) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetCapability(std::int32_t deviceId, std::uint32_t* capability) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (capability == nullptr) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(deviceId);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadShareOutputData(std::int32_t source, std::int32_t destination) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadShareOutputDataWithFunctionType(std::int32_t source, std::int32_t destination, std::uint32_t functionType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetVibrationForce(std::int32_t handle, const PadVibrationParam* param) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return PAD_ERROR_INVALID_ARG;
    if (SpecialHandle(handle)) return PAD_OK;
    const int port = PortOf(handle);
    if (port < 0) NotImplemented_nid_no_patch("scePadSetVibrationForce: a handle with no pad port");
    PadVibrationParam local{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&local, param, sizeof(local));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    Pad::SetVibration(port, local.large_motor, local.small_motor);
    return PAD_OK;
}

int APS5_VABI scePadGetVersionInfo(std::int32_t handle, PadVersionInfo* info) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(handle);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadGetControllerType(std::int32_t handle, std::int32_t* type) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (type == nullptr) return PAD_ERROR_INVALID_ARG;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    const std::int32_t controllerType = SpecialHandle(handle) ? PAD_CONTROLLER_TYPE_UNKNOWN : PAD_CONTROLLER_TYPE_DUAL_SENSE;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(type, &controllerType, sizeof(controllerType));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    return PAD_OK;
}

int APS5_VABI scePadDisableDataConversion(std::int32_t handle, bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (disabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadSetButtonRemappingInfo(std::int32_t handle, const PadButtonRemappingInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProcessPrivilegeOfButtonRemapping(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetForceIntercepted(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadIsLightBarBaseBrightnessControllable(std::int32_t handle) {
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (SpecialHandle(handle)) return 0;
    return 1;
}

int APS5_VABI scePadSwitchConnection(std::int32_t handle, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableTriggerEffect(std::int32_t handle, bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (SpecialHandle(handle)) return PAD_OK;
    Pad::SetTriggerEffectsDisabled(PortOf(handle), disabled);
    return PAD_OK;
}

int APS5_VABI scePadSetTriggerEffectStrength(std::int32_t handle, std::uint32_t strength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetTriggerEffectInternalState(std::int32_t handle, PadTriggerInternalState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetUserNumber(std::int32_t handle, std::int32_t number) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableTransportButtons(bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (disabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadSetLotusIrCodeSetStart(std::int32_t handle, std::uint8_t codeSet, std::uint16_t code, std::size_t dataSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLotusIrCodeSetData(std::int32_t handle, std::uint8_t packetIndex, const void* data, std::size_t dataSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLotusIrCodeSetEnd(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetLotusIrCodeSetResult(std::int32_t handle, std::int32_t* result, std::uint8_t* deviceResult) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLotusEnableIrKey(std::int32_t handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLotusDisableIrKey(std::int32_t handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetUsbDetectionState(std::int32_t deviceId, PadUsbDetectionState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadUiReadState(std::int32_t handle, PadUiData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadUiReadStateForTuning(std::int32_t handle, PadUiTuningData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetStickTriggerTuningMode(std::int32_t handle, std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
