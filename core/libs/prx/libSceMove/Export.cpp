#include "prx/libSceMove/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"

namespace {

constexpr int MOVE_OK = 0;
constexpr int MOVE_NO_CONTROLLER_CONNECTED = 1;
constexpr int MOVE_ERROR_NOT_INIT = static_cast<int>(0x80EE0001);
constexpr int MOVE_ERROR_ALREADY_INIT = static_cast<int>(0x80EE0002);
constexpr int MOVE_ERROR_INVALID_ARG = static_cast<int>(0x80EE0003);
constexpr int MOVE_ERROR_INVALID_HANDLE = static_cast<int>(0x80EE0004);
constexpr int MOVE_ERROR_ALREADY_OPENED = static_cast<int>(0x80EE0007);
constexpr int32_t MOVE_FIRST_HANDLE = 0x30B0100;
constexpr int32_t MOVE_HANDLE_STEP = 0x100;

std::mutex stateMutex;
bool initialized = false;
int32_t nextHandle = MOVE_FIRST_HANDLE;
struct Slot {
    int32_t type;
    int32_t index;
};

std::unordered_map<int32_t, Slot> openHandles;

int checkHandle(int32_t handle, bool validArguments = true) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return MOVE_ERROR_NOT_INIT;
    if (!validArguments) return MOVE_ERROR_INVALID_ARG;
    if (!openHandles.contains(handle)) return MOVE_ERROR_INVALID_HANDLE;
    return MOVE_OK;
}

}

extern "C" {

int APS5_VABI sceMoveInit(void) {
    std::lock_guard lock(stateMutex);
    if (initialized) return MOVE_ERROR_ALREADY_INIT;
    initialized = true;
    return MOVE_OK;
}

int APS5_VABI sceMoveTerm(void) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return MOVE_ERROR_NOT_INIT;
    initialized = false;
    openHandles.clear();
    return MOVE_OK;
}

int APS5_VABI sceMoveOpen(int32_t userId, int32_t type, int32_t index) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return MOVE_ERROR_NOT_INIT;
    if (userId != USER_SERVICE_INITIAL_USER_ID) NotImplemented_nid_no_patch(__func__);
    for (const auto& [openHandle, slot] : openHandles) {
        if (slot.type == type && slot.index == index) return MOVE_ERROR_ALREADY_OPENED;
    }
    const int32_t handle = nextHandle;
    nextHandle += MOVE_HANDLE_STEP;
    openHandles.emplace(handle, Slot{type, index});
    return handle;
}

int APS5_VABI sceMoveClose(int32_t handle) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return MOVE_ERROR_NOT_INIT;
    return openHandles.erase(handle) != 0 ? MOVE_OK : MOVE_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceMoveGetDeviceInfo(int32_t handle, void* info) {
    if (const int result = checkHandle(handle, info != nullptr); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveGetExtensionPortInfo(int32_t handle, void* data) {
    if (const int result = checkHandle(handle, data != nullptr); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveReadStateLatest(int32_t handle, void* data) {
    if (const int result = checkHandle(handle, data != nullptr); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveReadStateRecent(int32_t handle, int64_t timestamp, void* data, int32_t* outCount) {
    if (const int result = checkHandle(handle, timestamp >= 0 && data != nullptr && outCount != nullptr); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveSetVibration(int32_t handle, uint8_t motor) {
    (void)motor;
    if (const int result = checkHandle(handle); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveSetLightSphere(int32_t handle, uint8_t red, uint8_t green, uint8_t blue) {
    (void)red;
    (void)green;
    (void)blue;
    if (const int result = checkHandle(handle); result != MOVE_OK) return result;
    return MOVE_NO_CONTROLLER_CONNECTED;
}

int APS5_VABI sceMoveResetLightSphere(int32_t handle) {
    return checkHandle(handle);
}
int APS5_VABI sceMoveGetBluetoothAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveGetCalibrationInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveGetDeviceId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveIsReproductionModel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveIsValidHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveReadStateRecentForTracker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveSetCalibrationMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveSetExtensionPortOutput(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveSetLightSphereForTracker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveWriteCalibrationInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
