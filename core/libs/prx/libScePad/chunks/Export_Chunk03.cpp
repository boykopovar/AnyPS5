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


int APS5_VABI scePadSetStickTuningParam(std::int32_t handle, std::uint32_t stick, const PadStickTuningParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetTriggerTuningParam(std::int32_t handle, const PadTriggerTuningParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetProfileHunt(std::int32_t handle, std::int32_t profileId, PadHuntProfile* profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProfileHunt(std::int32_t handle, std::int32_t profileId, PadHuntProfile* profile, std::uint32_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadResetHunt(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetProfileInfo(std::int32_t deviceId, std::int32_t profileId, PadProfileInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetActiveProfileInfo(std::int32_t handle, PadActiveProfileInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableActiveProfileChange(std::int32_t handle, bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (disabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadDisableActiveProfileChangeByDeviceId(std::int32_t deviceId, bool disabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (disabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadSetActiveProfileSlot(std::int32_t deviceId, std::uint32_t slot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetSavedSettingInfo(std::int32_t handle, PadSavedSettingInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetSavedSettingInfo(std::int32_t handle, const PadSavedSettingInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVrControllerGetPhysicalUsbConnectionStatus(PadVrUsbConnectionStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadParseUniqueDataLeonardo(std::int32_t handle, const PadLeonardoData* data, PadLeonardoUniqueData* uniqueData) {
    (void)handle;
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (data == nullptr || uniqueData == nullptr) return PAD_ERROR_INVALID_ARG;
    const std::uint8_t zeroData[PAD_LEONARDO_UNIQUE_DATA_SIZE]{};
    const int savedErrno = errno;
    const bool copiedOut = GuestCopyOut_nid_no_patch(uniqueData, zeroData, sizeof(zeroData));
    errno = savedErrno;
    std::uint8_t probe = 0;
    const bool copiedIn = GuestCopyIn_nid_no_patch(&probe, data, 1);
    errno = savedErrno;
    if (!copiedOut || !copiedIn) return PAD_ERROR_INVALID_ARG;
    return PAD_ERROR_INVALID_HANDLE;
}

int APS5_VABI scePadGetControllerTypeLeonardo(std::int32_t deviceId, std::int32_t* controllerType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetControllerTypeLeonardo(std::int32_t deviceId, std::int32_t controllerType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetProfileLeonardo(std::int32_t handle, std::int32_t profileId, PadLeonardoProfile* profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProfileLeonardo(std::int32_t handle, std::int32_t profileId, PadLeonardoProfile* profile, std::uint32_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadResetLeonardo(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProfileEditMode(std::int32_t handle, bool enabled, std::uint32_t profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetProfileEditMode(std::int32_t handle, std::int32_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetProfileEditModeLeonardo(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetSelectButtonCustomizeMode(std::int32_t handle, bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetSelectButtonCustomizeMode(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetToggleAllReleaseLeonardo(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableControllerIndicator(std::int32_t handle) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableProfileIndicator(std::int32_t handle) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProfileIndicatorAnimation(std::int32_t handle, std::uint8_t animation) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadIsCoPilot(std::int32_t handle) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    (void)handle;
    return 0;
}

int APS5_VABI scePadSetIsCoPilot(std::int32_t handle, bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadRegisterCollaboInputMgrProcess(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetTelemetryInfo(std::int32_t handle, PadTelemetryInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetActiveTime(std::int32_t handle, std::int32_t type, std::uint64_t* activeTime) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (activeTime == nullptr) return PAD_ERROR_INVALID_ARG;
    const std::uint64_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(activeTime, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    static_cast<void>(type);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadGetDeviceId(std::int32_t handle, std::int32_t* deviceId) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (deviceId == nullptr) return PAD_ERROR_INVALID_ARG;
    const std::int32_t value = -1;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(deviceId, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadGetInfoByPortType(std::int32_t type, PadPortInfo* info) {
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if ((static_cast<std::uint32_t>(type) & ~2u) != 0) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadResetLightBarAll(void) {
    return scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD);
}

int APS5_VABI scePadResetLightBarAllByPortType(std::uint32_t portType) {
    if (portType != PAD_PORT_TYPE_STANDARD && portType != 1 && portType != PAD_PORT_TYPE_SPECIAL) return PAD_ERROR_INVALID_ARG;
    if (!Pad::IsInitialized()) return PAD_OK;
    const int type = static_cast<int>(portType);
    for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) if (OpenPortType(PAD_HANDLE_BASE + port) == type) scePadResetLightBar(PAD_HANDLE_BASE + port);
    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) if (OpenPortType(PAD_SPECIAL_HANDLE + slot) == type) scePadResetLightBar(PAD_SPECIAL_HANDLE + slot);
    return PAD_OK;
}

int APS5_VABI scePadSetLightBarForTracker(std::int32_t handle, const PadLightBarParam* color) {
    return scePadSetLightBar(handle, color);
}

int APS5_VABI scePadReadForTracker(std::int32_t handle, PadTrackerData* data, std::int32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadReadBlasterForTracker(std::int32_t handle, PadBlasterTrackerData* data, std::int32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadResetOrientationForTracker(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetMotionTimerUnit(std::int32_t handle, float* unit) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (unit == nullptr) return PAD_ERROR_INVALID_ARG;
    const float localUnit = 0.0f;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(unit, &localUnit, sizeof(localUnit))) {
        errno = savedErrno;
        return PAD_ERROR_INVALID_ARG;
    }
    errno = savedErrno;
    if (!OpenHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

int APS5_VABI scePadSetVrTrackingMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetMotionSensorPosition(std::int32_t handle, PadMotionSensorPosition* position) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (position == nullptr || !GuestWritable_nid_no_patch(position, sizeof(*position))) return PAD_ERROR_INVALID_ARG;
    if (!OpenHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

int APS5_VABI scePadGetSphereRadius(std::int32_t handle, float* radius) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (radius == nullptr || !GuestWritable_nid_no_patch(radius, sizeof(*radius))) return PAD_ERROR_INVALID_ARG;
    if (OpenPortType(handle) != PAD_PORT_TYPE_SPECIAL) return PAD_ERROR_INVALID_HANDLE;
    return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

bool APS5_VABI scePadIsValidHandle(std::int32_t handle, std::int32_t portType) {
    const int type = OpenPortType(handle);
    return Pad::IsInitialized() && type >= 0 && type == portType;
}

bool APS5_VABI scePadIsDS4Connected(std::int32_t handle) {
    (void)handle;
    return false;
}

bool APS5_VABI scePadIsBlasterConnected(std::int32_t handle) {
    (void)handle;
    return false;
}

bool APS5_VABI scePadIsMoveConnected(std::int32_t handle) {
    (void)handle;
    return false;
}

int APS5_VABI scePadGetBluetoothAddress(std::int32_t handle, std::uint64_t* address) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (address == nullptr || !GuestWritable_nid_no_patch(address, sizeof(*address))) return PAD_ERROR_INVALID_ARG;
    if (!OpenHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

int APS5_VABI scePadIsMoveReproductionModel(std::int32_t handle) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    (void)handle;
    return 0;
}

int APS5_VABI scePadReadAstonForTracker(std::int32_t handle, PadAstonTrackerData* data, std::int32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetAstonModel(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadMbusInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadMbusTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadConnectPort(std::int32_t userId, std::int32_t portType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisconnectPort(std::int32_t userId, std::int32_t portType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisconnectDevice(std::int32_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetDeviceInfo(std::int32_t deviceId, PadDeviceInfo* info) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(deviceId);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadSetProcessFocus(const PadProcessFocusParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVertualDeviceAddDevice(const PadVirtualDeviceParameter* parameter, PadVirtualDeviceId* deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceAddDevice(const PadVirtualDeviceParameter* parameter, PadVirtualDeviceId* deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceDeleteDevice(std::int32_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceInsertData(std::int32_t deviceId, const PadData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceInsertTriggerInternalState(std::int32_t deviceId, const PadVirtualTriggerInternalState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceGetRemoteSetting(std::int32_t deviceId, PadRemoteSetting* setting) {
    (void)deviceId;
    (void)setting;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceSetTriggerEffectSetting(std::int32_t handle, const PadVirtualTriggerSetting* setting) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVirtualDeviceDisableButtonRemapping(bool disabled) {
    (void)disabled;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetShareButtonMaskForRemotePlay(std::uint8_t mask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetPS4BcVibrationMode(std::uint8_t mode) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    static_cast<void>(mode);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadSetVirtualDeviceComponentInformation(std::int32_t deviceId, const PadVirtualDeviceComponentInformation* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnablePs4CompatibleMode(std::int32_t handle, bool enabled) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
    if (enabled) NotImplemented_nid_no_patch(__func__);
    return PAD_OK;
}

int APS5_VABI scePadGetLicenseControllerInformation(std::int32_t handle, PadLicenseControllerInformation* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
