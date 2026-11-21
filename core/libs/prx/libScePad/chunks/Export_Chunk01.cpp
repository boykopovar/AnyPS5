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
 PadDeviceClassExtendedInformation localInfo{};
 localInfo.deviceClass = SpecialHandle(handle) ? -1 : PAD_DEVICE_CLASS_STANDARD;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return PAD_OK;
}

int APS5_VABI scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (data == nullptr || class_data == nullptr) return PAD_ERROR_INVALID_ARG;
 PadDeviceClassData local{};
 if (!SpecialHandle(handle)) {
  local.deviceClass = PAD_DEVICE_CLASS_STANDARD;
  bool connected = false;
  const int savedErrno = errno;
  const bool copiedIn = GuestCopyIn_nid_no_patch(&connected, &data->connected, sizeof(connected));
  errno = savedErrno;
  if (!copiedIn) return PAD_ERROR_INVALID_ARG;
  local.dataValid = connected;
 }
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(class_data, &local, sizeof(local));
 errno = savedErrno;
 return copied ? PAD_OK : PAD_ERROR_INVALID_ARG;
}

int APS5_VABI scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 if (!KnownHandle(handle)) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 if (info == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 PadControllerInformation localInfo{};
 if (SpecialHandle(handle)) {
  const int savedErrno = errno;
  const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
  errno = savedErrno;
  if (!copied) return PAD_ERROR_INVALID_ARG;
  return PAD_OK;
 }
 localInfo.touchPadInfo.pixelDensity = 44.86f;
 localInfo.touchPadInfo.resolution.x = kTouchPadWidth;
 localInfo.touchPadInfo.resolution.y = kTouchPadHeight;
 localInfo.stickInfo.deadZoneLeft = 13;
 localInfo.stickInfo.deadZoneRight = 13;
 localInfo.connectionType = PAD_CONNECTION_TYPE_LOCAL;
 const bool connected = Pad::IsConnected(PortOf(handle));
 localInfo.connectedCount = connected ? 1 : 0;
 localInfo.connected = connected;
 localInfo.deviceClass = PAD_DEVICE_CLASS_STANDARD;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
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
 PadInfo local{};
 local.maxConnectCount = PAD_MAX_CONTROLLERS;
 std::uint8_t count = 0;
 for (int port = 0; port < PAD_MAX_CONTROLLERS; ++port) {
  if (Pad::IsConnected(port)) {
   PadInfo::PadTypeInfo& slot = local.padTypeInfo[count];
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
 local.connectedCount[0] = count;
 local.connectedCount[1] = 0;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &local, sizeof(local));
 errno = savedErrno;
 return copied ? PAD_OK : PAD_ERROR_INVALID_ARG;
}

int APS5_VABI scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 PadTriggerEffectStateInformation localInfo{};
 if (!SpecialHandle(handle)) {
  PadGetTriggerEffectState_nid_postfix(PortOf(handle), localInfo.state);
 }
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return PAD_OK;
}

int APS5_VABI scePadGetExtControllerInformation(int handle, void* info) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 if (!SpecialHandle(handle)) return PAD_ERROR_NOT_PERMITTED;
 constexpr std::array<std::uint8_t, 0x2c> zeroed{};
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, zeroed.data(), zeroed.size());
 errno = savedErrno;
 return copied ? PAD_OK : PAD_ERROR_INVALID_ARG;
}

int APS5_VABI scePadSetProcessPrivilege(int privilege) {
 (void)privilege;
 return PAD_OK;
}

int APS5_VABI scePadSetParticularMode(bool mode) {
 g_particularMode.store(mode, std::memory_order_release);
 return PAD_OK;
}

int APS5_VABI scePadGetParticularMode(void) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 return g_particularMode.load(std::memory_order_acquire) ? 1 : 0;
}

int APS5_VABI scePadGetControllerBusType(int handle, int* busType) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (busType == nullptr) return PAD_ERROR_INVALID_ARG;
 const int device = DeviceOf(handle);
 if (device != PAD_OK) return device;
 const int type = PAD_CONNECT_TYPE_BLUETOOTH;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(busType, &type, sizeof(type));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return PAD_OK;
}

int APS5_VABI scePadGetJackState(int handle, int* state) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (state == nullptr) return PAD_ERROR_INVALID_ARG;
 const int device = DeviceOf(handle);
 if (device != PAD_OK) return device;
 const int jackState = 0;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(state, &jackState, sizeof(jackState));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return PAD_OK;
}

int APS5_VABI scePadIsControllerUpdateRequired(int handle) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 return DeviceOf(handle);
}

int APS5_VABI scePadGetContainerIdInformation(int handle, PadContainerIdInformation* info) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (info == nullptr) return PAD_ERROR_INVALID_ARG;
 const int device = DeviceOf(handle);
 if (device != PAD_OK) return device;
 PadContainerIdInformation localInfo{};
 localInfo.size = 0;
 localInfo.id[0] = '\0';
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, offsetof(PadContainerIdInformation, id) + 1);
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return PAD_OK;
}

int APS5_VABI scePadInit_nid_postfix(void) {
 Pad::Initialize();
 g_initialized.store(true, std::memory_order_release);
 return PAD_OK;
}

int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param) {
 (void)param;
 const bool unknownType = type == 3 || type == 4;
 const bool knownType = type == PAD_PORT_TYPE_STANDARD || type == PAD_PORT_TYPE_SPECIAL || SystemRemote(userId, type);
 if (!unknownType && !knownType) return PAD_ERROR_INVALID_ARG;
 if (userId == PAD_USER_ID_INVALID) return PAD_ERROR_INVALID_USER_ID;
 if (unknownType) return PAD_ERROR_INVALID_ARG;
 if (index != 0) return PAD_ERROR_INVALID_PORT_INDEX;
 if (type == PAD_PORT_TYPE_SPECIAL) {
  if (SpecialHandleOf(userId) != PAD_ERROR_DEVICE_NO_HANDLE) return PAD_ERROR_ALREADY_OPENED;
  for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
   int free = 0;
   if (g_specialUsers[slot].compare_exchange_strong(free, userId)) return PAD_SPECIAL_HANDLE + slot;
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
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (data == nullptr || num < 1 || num > 64) return PAD_ERROR_INVALID_ARG;
 if (!KnownHandle(handle) && !RemoteHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 std::array<PadData, 64> localData{};
 int count = 1;
 if (RemoteHandle(handle)) localData[0] = PadData{};
 else if (SpecialHandle(handle)) localData[0] = SpecialData();
 else count = Pad::Read(localData.data(), num, PortOf(handle));
 if (count <= 0) return count;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(data, localData.data(), static_cast<std::size_t>(count) * sizeof(PadData));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 return count;
}

int APS5_VABI scePadReadState(int handle, PadData* data) {
 if (data == nullptr) return PAD_ERROR_INVALID_ARG;
 PadData localData;
 WriteDefaultData(&localData);
 const int savedErrno = errno;
 if (!GuestCopyOut_nid_no_patch(data, &localData, sizeof(localData))) {
  errno = savedErrno;
  return PAD_ERROR_INVALID_ARG;
 }
 errno = savedErrno;
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 if (!KnownHandle(handle) && !RemoteHandle(handle)) return PAD_ERROR_INVALID_HANDLE;

 localData = RemoteHandle(handle) ? PadData{} : SpecialHandle(handle) ? SpecialData() : Pad::ReadState(PortOf(handle));
 const int savedErrno2 = errno;
 if (!GuestCopyOut_nid_no_patch(data, &localData, sizeof(localData))) {
  errno = savedErrno2;
  return PAD_ERROR_INVALID_ARG;
 }
 errno = savedErrno2;

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

int APS5_VABI scePadSetConnection(std::int32_t handle, bool connected) {
 if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePadSetLightBar(int handle, const PadLightBarParam* param) {
 if (!KnownHandle(handle)) return PAD_ERROR_INVALID_HANDLE;
 if (param == nullptr) return PAD_ERROR_INVALID_ARG;
 if (SpecialHandle(handle)) return PAD_OK;
 PadLightBarParam local{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&local, param, sizeof(local));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 Pad::SetLightBar(PortOf(handle), true, local.r, local.g, local.b);
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
 std::array<std::uint8_t, 120> local{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(local.data(), param, local.size());
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 const std::uint8_t* bytes = local.data();
 const std::uint8_t mask = bytes[0];
 if ((mask & ~0x3u) != 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::uint32_t mode[2] = {};
 for (int trigger = 0; trigger < 2; ++trigger) std::memcpy(&mode[trigger], bytes + 8 + 56 * trigger, 4);
 for (int trigger = 0; trigger < 2; ++trigger) {
  if ((mask & (1u << trigger)) != 0 && mode[trigger] > 6) return PAD_ERROR_INVALID_ARG;
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
 PadVibrationParam local{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&local, param, sizeof(local));
 errno = savedErrno;
 if (!copied) return PAD_ERROR_INVALID_ARG;
 Pad::SetVibration(PortOf(handle), local.large_motor, local.small_motor);
 return PAD_OK;
}

}
