#include <cstddef>
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
static_assert(sizeof(PadButtonRemappingInfo) == 16);
static_assert(offsetof(PadButtonRemappingInfo, mode) == 15);

namespace {

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007);
constexpr int PAD_ERROR_DEVICE_NO_HANDLE = static_cast<int>(0x80920008);
constexpr int PAD_ERROR_ALREADY_OPENED = static_cast<int>(0x80920004);
constexpr int PAD_ERROR_UNKNOWN_USER = static_cast<int>(0x809b0081);

std::array<bool, PAD_MAX_CONTROLLERS> g_opened{false};
std::array<bool, PAD_MAX_CONTROLLERS> g_everOpened{false};
bool g_remoteOpened = false;
std::array<int, PAD_SPECIAL_HANDLE_COUNT> g_specialUsers{};
std::atomic_bool g_initialized{false};

bool SpecialHandle(int handle) {
    const int slot = handle - PAD_SPECIAL_HANDLE;
    return slot >= 0 && slot < PAD_SPECIAL_HANDLE_COUNT && g_specialUsers[slot] != 0;
}

int SpecialHandleOf(int userId) {
    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        if (g_specialUsers[slot] == userId) return PAD_SPECIAL_HANDLE + slot;
    }
    return PAD_ERROR_DEVICE_NO_HANDLE;
}

int PortOf(int handle) {
    const int port = handle - PAD_HANDLE_BASE;
    if (port < 0 || port >= PAD_MAX_CONTROLLERS || !g_everOpened[port]) return -1;
    return port;
}

bool KnownHandle(int handle) {
    return PortOf(handle) >= 0 || SpecialHandle(handle);
}

PadData SpecialData() {
    PadData data{};
    data.left_stick_x = data.left_stick_y = data.right_stick_x = data.right_stick_y = 0x80;
    data.orientation_w = 1.0f;
    data.acceleration_y = 1.0f;
    return data;
}

bool SystemRemote(int userId, int type) {
    return userId == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE;
}

bool RemoteHandle(int handle) {
    return g_remoteOpened && handle == PAD_REMOTE_HANDLE;
}

int PortForUser(int userId, int type, int index) {
    if (index != 0) return -1;
    if (type == PAD_PORT_TYPE_STANDARD) {
        const int port = userId - 0x10000000;
        if (port >= 0 && port < PAD_MAX_CONTROLLERS) return port;
    }
    return -1;
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

extern "C" {

int APS5_VABI scePadClose_nid_postfix(int handle) {
 if (RemoteHandle(handle)) {
  g_remoteOpened = false;
  return PAD_OK;
 }
 if (SpecialHandle(handle)) {
  g_specialUsers[handle - PAD_SPECIAL_HANDLE] = 0;
  return PAD_OK;
 }
 const int port = PortOf(handle);
 if (port < 0) return PAD_ERROR_INVALID_HANDLE;
 g_opened[port] = false;
 return PAD_OK;
}

int APS5_VABI scePadVrControllerSetVibration(std::int32_t handle, std::uint8_t intensity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVrControllerSetVibrationMode(std::int32_t handle, std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadVrControllerReadCombinedState(std::int32_t leftHandle, std::int32_t rightHandle, PadVrControllerCombinedState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 info->deviceClass = SpecialHandle(handle) ? -1 : PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr || class_data == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(class_data, 0, sizeof(*class_data));
 if (SpecialHandle(handle)) return PAD_OK;
 class_data->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 class_data->dataValid = data->connected;
 return PAD_OK;
}

int APS5_VABI scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 if (!KnownHandle(handle)) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 if (info == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::memset(info, 0, sizeof(*info));
 if (SpecialHandle(handle)) return PAD_OK;
 info->touchPadInfo.pixelDensity = 44.86f;
 info->touchPadInfo.resolution.x = kTouchPadWidth;
 info->touchPadInfo.resolution.y = kTouchPadHeight;
 info->stickInfo.deadZoneLeft = 13;
 info->stickInfo.deadZoneRight = 13;
 info->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 info->connectedCount = 1;
 info->connected = true;
 info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadGetHandle(int user_id, int type, int index) {
 if (!ValidPort(user_id, type, index)) return PAD_ERROR_DEVICE_NO_HANDLE;
 if (type == PAD_PORT_TYPE_SPECIAL) return SpecialHandleOf(user_id);
 if (SystemRemote(user_id, type)) return g_remoteOpened ? PAD_REMOTE_HANDLE : PAD_ERROR_DEVICE_NO_HANDLE;
 const int port = PortForUser(user_id, type, index);
 if (port < 0 || !g_opened[port]) return PAD_ERROR_DEVICE_NO_HANDLE;
 return PAD_HANDLE_BASE + port;
}

int APS5_VABI scePadGetInfo_nid_postfix(PadInfo* info) {
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 info->maxConnectCount = PAD_MAX_CONTROLLERS;
 std::uint8_t count = 0;
 for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
  if (Pad::IsConnected(port)) {
   PadInfo::PadTypeInfo& slot = info->padTypeInfo[count];
   slot.connectPort = static_cast<std::uint8_t>(port);
   slot.status = 1;
   slot.deviceType = PAD_DEVICE_TYPE_DUAL_SENSE;
   slot.connectType = PAD_CONNECT_TYPE_BLUETOOTH;
   std::uint8_t mac[6];
   std::memcpy(mac, PAD_BD_ADDRESS, sizeof(mac));
   mac[5] = static_cast<std::uint8_t>(mac[5] + port);
   std::memcpy(slot.bluetoothMacAddress, mac, sizeof(slot.bluetoothMacAddress));
   ++count;
  }
 }
 if (count == 0) {
  PadInfo::PadTypeInfo& slot = info->padTypeInfo[0];
  slot.connectPort = 0;
  slot.status = 1;
  slot.deviceType = PAD_DEVICE_TYPE_DUAL_SENSE;
  slot.connectType = PAD_CONNECT_TYPE_BLUETOOTH;
  std::memcpy(slot.bluetoothMacAddress, PAD_BD_ADDRESS, sizeof(slot.bluetoothMacAddress));
  count = 1;
 }
 info->connectedCount[0] = count;
 info->connectedCount[1] = 0;
 return PAD_OK;
}

int APS5_VABI scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, sizeof(*info));
 return PAD_OK;
}

int APS5_VABI scePadGetExtControllerInformation(int handle, void* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 std::memset(info, 0, 0x2c);
 if (SpecialHandle(handle)) return PAD_OK;
 PadControllerInformation* base = static_cast<PadControllerInformation*>(info);
 base->touchPadInfo.pixelDensity = 44.86f;
 base->touchPadInfo.resolution.x = kTouchPadWidth;
 base->touchPadInfo.resolution.y = kTouchPadHeight;
 base->stickInfo.deadZoneLeft = 13;
 base->stickInfo.deadZoneRight = 13;
 base->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 base->connectedCount = 1;
 base->connected = true;
 base->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int APS5_VABI scePadSetProcessPrivilege(int privilege) {
 (void)privilege;
 return PAD_OK;
}

int APS5_VABI scePadSetParticularMode(bool mode) {
 (void)mode;
 return PAD_OK;
}

int APS5_VABI scePadInit_nid_postfix(void) {
 Pad::Initialize();
 g_initialized.store(true, std::memory_order_release);
 return PAD_OK;
}

int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param) {
 (void)param;
 if (!ValidPort(userId, type, index)) {
  return PAD_ERROR_INVALID_ARG;
 }
 if (type == PAD_PORT_TYPE_SPECIAL) {
  if (SpecialHandleOf(userId) != PAD_ERROR_DEVICE_NO_HANDLE) return PAD_ERROR_ALREADY_OPENED;
  for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
   if (g_specialUsers[slot] == 0) {
    g_specialUsers[slot] = userId;
    return PAD_SPECIAL_HANDLE + slot;
   }
  }
  return PAD_ERROR_UNKNOWN_USER;
 }
 if (SystemRemote(userId, type)) {
  g_remoteOpened = true;
  return PAD_REMOTE_HANDLE;
 }
 const int port = PortForUser(userId, type, index);
 if (port < 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 g_opened[port] = true;
 g_everOpened[port] = true;
 return PAD_HANDLE_BASE + port;
}

int APS5_VABI scePadOpenExt(int userId, int type, int index, const void* param) {
 if (!ValidPort(userId, type, index) || param == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 if (type != PAD_PORT_TYPE_SPECIAL) NotImplemented_nid_no_patch(__func__);
 return PAD_ERROR_DEVICE_NOT_CONNECTED;
}

int APS5_VABI scePadReadExt(std::int32_t handle, PadDataExtended* data, std::int32_t count) { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadGetFeatureReport(std::int32_t handle, std::uint8_t reportId, void* data, std::size_t size) { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadSetFeatureReport(std::int32_t handle, std::uint8_t reportId, const void* data, std::size_t size) { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadOutputReport(std::int32_t handle, std::uint8_t reportId, const void* data, std::uint64_t size) { NotImplemented_nid_no_patch(__func__); return 0; }

int APS5_VABI scePadRead_nid_postfix(int handle, PadData* data, int num) {
 if (!KnownHandle(handle) && !RemoteHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr || num <= 0) return PAD_ERROR_INVALID_ARG;
 if (RemoteHandle(handle)) {
  data[0] = PadData{};
  return 1;
 }
 if (SpecialHandle(handle)) {
  data[0] = SpecialData();
  return 1;
 }
 return Pad::Read(data, num, PortOf(handle));
}

int APS5_VABI scePadReadState(int handle, PadData* data) {
 if (!KnownHandle(handle) && !RemoteHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr) return PAD_ERROR_INVALID_ARG;

 *data = RemoteHandle(handle) ? PadData{} : SpecialHandle(handle) ? SpecialData() : Pad::ReadState(PortOf(handle));

 return 0;
}

int APS5_VABI scePadResetLightBar(int handle) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetLightBar(PortOf(handle), false, 0, 0, 0);
 return PAD_OK;
}

int APS5_VABI scePadResetOrientation(int handle) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::ResetOrientation(PortOf(handle));
 return PAD_OK;
}

int APS5_VABI scePadSetAngularVelocityDeadbandState(int handle, bool enable) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetAngularVelocityDeadband(PortOf(handle), enable);
 return PAD_OK;
}

int APS5_VABI scePadSetLightBar(int handle, const PadLightBarParam* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetLightBar(PortOf(handle), true, param->r, param->g, param->b);
 return PAD_OK;
}

int APS5_VABI scePadSetMotionSensorState(int handle, bool enable) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetMotionEnabled(PortOf(handle), enable);
 return PAD_OK;
}

int APS5_VABI scePadSetTiltCorrectionState(int handle, bool enabled) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetTiltCorrection(PortOf(handle), enabled);
 return PAD_OK;
}

int APS5_VABI scePadSetTriggerEffect(int handle, const void* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 const std::uint8_t* bytes = static_cast<const std::uint8_t*>(param);
 const std::uint8_t mask = bytes[0];
 if ((mask & ~0x3u) != 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::uint32_t mode[2] = {};
 for (int trigger = 0; trigger < 2; ++trigger) std::memcpy(&mode[trigger], bytes + 8 + 56 * trigger, 4);
 if (mode[0] > 6 || mode[1] > 6) {
  return PAD_ERROR_INVALID_ARG;
 }
 const int port = PortOf(handle);
 for (int trigger = 0; trigger < 2; ++trigger) {
  if ((mask & (1u << trigger)) != 0) Pad::SetTriggerCommand(port, trigger, bytes + 8 + 56 * trigger);
 }
 return PAD_OK;
}

int APS5_VABI scePadSetVibration(int handle, const PadVibrationParam* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetVibration(PortOf(handle), param->large_motor, param->small_motor);
 return PAD_OK;
}

int APS5_VABI scePadSetVibrationMode(int handle, int mode) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (mode <= 0) return PAD_ERROR_INVALID_ARG;
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (mode > 3 && mode != 5) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 Pad::SetVibrationMode(PortOf(handle), mode);
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
 *remote = false;
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
    *state = 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetExtensionReport(std::int32_t handle, const PadExtensionReport* report) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetExtensionUnitInfo(std::int32_t handle, PadExtensionUnitInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableExtensionPort(bool enabled) {
    return 0;
}

int APS5_VABI scePadEnableSpecificDeviceClass(std::int32_t deviceClass) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadOpenExt2(std::uint64_t deviceId, std::int32_t type, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetVibrationStrength(std::int32_t handle, std::uint32_t strength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLightBarBaseBrightness(std::int32_t handle, std::uint32_t brightness) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLightBarBlinking(std::int32_t handle, const PadLightBarBlinkingParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI scePadGetIdleCount(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetControllerIdleCount(std::int32_t handle, std::uint64_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableAutoDetect(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnablePlayPauseEmulation(bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableTrackingLedBatterySaver(bool disabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLoginUserNumber(std::int32_t number) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetCapability(std::int32_t deviceId, std::uint32_t* capability) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetVersionInfo(std::int32_t handle, PadVersionInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetControllerType(std::int32_t handle, std::int32_t* type) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (type == nullptr) return PAD_ERROR_INVALID_ARG;
    if (PortOf(handle) < 0) return PAD_ERROR_INVALID_HANDLE;
    *type = PAD_CONTROLLER_TYPE_DUAL_SENSE;
    return PAD_OK;
}

int APS5_VABI scePadDisableDataConversion(std::int32_t handle, bool disabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSwitchConnection(std::int32_t handle, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableTriggerEffect(std::int32_t handle, bool disabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadDisableActiveProfileChangeByDeviceId(std::int32_t deviceId, bool disabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnableProfileIndicator(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetProfileIndicatorAnimation(std::int32_t handle, std::uint8_t animation) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadIsCoPilot(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetDeviceId(std::int32_t handle, std::int32_t* deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetInfoByPortType(std::int32_t type, PadPortInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadResetLightBarAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadResetLightBarAllByPortType(std::uint32_t portType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetLightBarForTracker(std::int32_t handle, const PadLightBarParam* color) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetVrTrackingMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetMotionSensorPosition(std::int32_t handle, PadMotionSensorPosition* position) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetSphereRadius(std::int32_t handle, float* radius) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI scePadIsValidHandle(std::int32_t handle, std::int32_t portType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadIsMoveReproductionModel(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetVirtualDeviceComponentInformation(std::int32_t deviceId, const PadVirtualDeviceComponentInformation* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadEnablePs4CompatibleMode(std::int32_t handle, bool enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetLicenseControllerInformation(std::int32_t handle, PadLicenseControllerInformation* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadReadHistory(std::int32_t handle, PadHistoryData* data, std::int32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadStartRecording(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadStopRecording(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadRemoteControlInsertData(const std::uint8_t* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadRemoteControlInsertDataForAston(const std::uint8_t* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadGetFeatureReport2(std::int32_t handle, std::uint8_t reportId, void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetFeatureReport2(std::int32_t handle, std::uint8_t reportId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadSetMuteLed(std::int32_t handle, std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateIsReady(std::int32_t deviceId, std::int32_t* ready) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateGetStatus(std::int32_t deviceId, std::int32_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateSendData(std::int32_t deviceId, const PadUpdateData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateResetBond(std::int32_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateGetReconnectStatus(std::int32_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadBondUpdateTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateIsUpdateNeeded2(std::int32_t deviceId, const std::uint32_t* version, std::uint16_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateIsReady(std::int32_t deviceId, std::int32_t* ready) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateGetStatus(std::int32_t deviceId, std::int32_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateSendData(std::int32_t deviceId, const PadUpdateData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateResetController(std::int32_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateGetReconnectStatus(std::int32_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadControllerUpdateTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
