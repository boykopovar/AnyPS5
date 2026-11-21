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


int APS5_VABI scePadReadHistory(std::int32_t handle, PadHistoryData* data, std::uint32_t size) {
    if (!Pad::IsInitialized()) return PAD_ERROR_NOT_INITIALIZED;
    if (size != PAD_HISTORY_BUFFER_SIZE) return PAD_ERROR_INVALID_ARG;
    static_cast<void>(handle);
    static_cast<void>(data);
    NotImplemented_nid_no_patch(__func__);
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
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadBondUpdateInit: the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateIsReady(std::int32_t deviceId, std::int32_t* ready) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (ready == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadBondUpdateIsReady: the driver answer behind the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter) {
    (void)handle;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (parameter == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadBondUpdateStart: the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateGetStatus(std::int32_t deviceId, std::int32_t* status) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (status == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadBondUpdateGetStatus: the driver answer behind the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateSendData(std::int32_t deviceId, const PadUpdateData* data) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (data == nullptr) return PAD_ERROR_INVALID_ARG;
    std::size_t size = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&size, data, sizeof(size));
    errno = savedErrno;
    if (!copied || size != 0x8000) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadBondUpdateSendData: the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateResetBond(std::int32_t deviceId) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadBondUpdateResetBond: the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateGetReconnectStatus(std::int32_t* status) {
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (status == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadBondUpdateGetReconnectStatus: the driver answer behind the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadBondUpdateTerm(void) {
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadBondUpdateTerm: the bond-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateInit(void) {
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadControllerUpdateInit: the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateIsUpdateNeeded2(std::int32_t deviceId, const std::uint32_t* version, std::uint16_t* status) {
    (void)deviceId;
    if (status == nullptr) return PAD_ERROR_INVALID_ARG;
    const std::uint16_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return PAD_ERROR_INVALID_ARG;
    if (version == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateIsUpdateNeeded2: the driver answer behind the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateIsReady(std::int32_t deviceId, std::int32_t* ready) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (ready == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateIsReady: the driver answer behind the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter) {
    (void)handle;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (parameter == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateStart: the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateGetStatus(std::int32_t deviceId, std::int32_t* status) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (status == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateGetStatus: the driver answer behind the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateSendData(std::int32_t deviceId, const PadUpdateData* data) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (data == nullptr) return PAD_ERROR_INVALID_ARG;
    std::size_t size = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&size, data, sizeof(size));
    errno = savedErrno;
    if (!copied || size != 0x8000) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateSendData: the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateResetController(std::int32_t deviceId) {
    (void)deviceId;
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadControllerUpdateResetController: the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateGetReconnectStatus(std::int32_t* status) {
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    if (status == nullptr) return PAD_ERROR_INVALID_ARG;
    NotImplemented_nid_no_patch("scePadControllerUpdateGetReconnectStatus: the driver answer behind the controller-update ioctl");
    return 0;
}

int APS5_VABI scePadControllerUpdateTerm(void) {
    if (!g_initialized.load(std::memory_order_acquire)) return PAD_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("scePadControllerUpdateTerm: the controller-update ioctl");
    return 0;
}

}
