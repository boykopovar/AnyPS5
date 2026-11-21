#include "prx/libSceBluetoothHid/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>
#include <string>
#include <cerrno>
#include <mutex>

static_assert(sizeof(BluetoothHidThreadParam) == 24);
static_assert(offsetof(BluetoothHidThreadParam, affinityMask) == 8);
static_assert(offsetof(BluetoothHidThreadParam, reserved) == 16);
static_assert(sizeof(BluetoothHidParam) == 16);
static_assert(sizeof(BluetoothHidEvent) == 104);
static_assert(offsetof(BluetoothHidEvent, reportSize) == 14);
static_assert(offsetof(BluetoothHidEvent, report) == 24);

namespace {
std::mutex stateMutex;
bool initialized = false;
bool callbackRegistered = false;
constexpr std::int32_t InvalidArgument = static_cast<std::int32_t>(0x81540001u);
constexpr std::int32_t NotInitialized = static_cast<std::int32_t>(0x81540004u);
constexpr std::int32_t NoCallback = static_cast<std::int32_t>(0x81540006u);
}

extern "C" {

std::int32_t APS5_VABI sceBluetoothHidInit() {
    std::lock_guard lock(stateMutex);
    initialized = true;
    return 0;
}

std::int32_t APS5_VABI sceBluetoothHidRegisterCallback(BluetoothHidCallback callback, void* userData, const BluetoothHidParam* param) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    if (callback == nullptr) return InvalidArgument;
    if (param != nullptr) {
        BluetoothHidParam localParam{};
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
        errno = savedErrno;
        if (!copied) return InvalidArgument;
        if (localParam.reserved != 0) return InvalidArgument;
        if (localParam.threadParam != nullptr) {
            BluetoothHidThreadParam localThreadParam{};
            const int savedErrnoThread = errno;
            const bool copiedThread = GuestCopyIn_nid_no_patch(&localThreadParam, localParam.threadParam, sizeof(localThreadParam));
            errno = savedErrnoThread;
            if (!copiedThread) return InvalidArgument;
            if (localThreadParam.reserved != 0) return InvalidArgument;
            throw std::runtime_error("sceBluetoothHidRegisterCallback: custom worker scheduling is not implemented");
        }
    }
    (void)userData;
    callbackRegistered = true;
    return 0;
}

std::int32_t APS5_VABI sceBluetoothHidUnregisterCallback() {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    if (!callbackRegistered) return NoCallback;
    callbackRegistered = false;
    return 0;
}

std::int32_t APS5_VABI sceBluetoothHidRegisterDevice(std::uint16_t vendorId, std::uint16_t productId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    if (!callbackRegistered) return NoCallback;
    (void)vendorId;
    (void)productId;
    return 0;
}

std::int32_t APS5_VABI sceBluetoothHidUnregisterDevice(std::uint16_t vendorId, std::uint16_t productId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    (void)vendorId;
    (void)productId;
    return 0;
}

int APS5_VABI sceBluetoothHidGetDeviceName(std::uint64_t deviceHandle, BluetoothHidDeviceName* name) {
    std::lock_guard lock(stateMutex);
    (void)deviceHandle;
    if (!initialized) return NotInitialized;
    if (name == nullptr) return InvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetInputReport(std::uint64_t deviceHandle, std::uint8_t reportId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidSetOutputReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidDebugGetVersion() {
    return 0x1e;
}

int APS5_VABI sceBluetoothHidDisconnectDevice(std::int32_t deviceId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetDeviceInfo(std::int32_t deviceId, BluetoothHidDeviceInfo* info) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetReportDescriptor(std::uint64_t deviceHandle, void* descriptor, std::size_t size) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidInterruptOutput(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidParamInitialize(BluetoothHidParam* param, const BluetoothHidThreadParam* threadParam) {
    if (param == nullptr) return InvalidArgument;
    BluetoothHidParam localParam{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) {
        errno = savedErrno;
        return InvalidArgument;
    }
    localParam.threadParam = threadParam;
    localParam.reserved = 0;
    if (!GuestCopyOut_nid_no_patch(param, &localParam, sizeof(localParam))) {
        errno = savedErrno;
        return InvalidArgument;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceBluetoothHidSetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return NotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidThreadParamInitialize(BluetoothHidThreadParam* param, std::int32_t priority, std::uint64_t affinityMask) {
    if (param == nullptr) return InvalidArgument;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(&param->priority, &priority, sizeof(priority))) {
        errno = savedErrno;
        return InvalidArgument;
    }
    if (!GuestCopyOut_nid_no_patch(&param->affinityMask, &affinityMask, sizeof(affinityMask))) {
        errno = savedErrno;
        return InvalidArgument;
    }
    const std::uint32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(&param->reserved, &zero, sizeof(zero))) {
        errno = savedErrno;
        return InvalidArgument;
    }
    errno = savedErrno;
    return 0;
}

}
