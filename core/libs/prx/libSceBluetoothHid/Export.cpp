#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <string>
#include <mutex>

struct BluetoothHidThreadParam {
    std::int32_t priority;
    std::uint32_t padding;
    std::uint64_t affinityMask;
    std::uint32_t reserved;
    std::uint32_t tailPadding;
};

struct BluetoothHidParam {
    const BluetoothHidThreadParam* threadParam;
    std::uint32_t reserved;
    std::uint32_t padding;
};

struct BluetoothHidEvent {
    std::uint64_t deviceHandle;
    std::uint16_t vendorId;
    std::uint16_t productId;
    std::uint8_t type;
    std::uint8_t status;
    std::uint16_t reportSize;
    std::uint8_t reserved[8];
    std::uint8_t report[78];
    std::uint8_t padding[2];
};

struct BluetoothHidDeviceInfo;
struct BluetoothHidDeviceName;
using BluetoothHidCallback = void (APS5_VABI *)(const BluetoothHidEvent*, std::int32_t, void*);

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
        if (param->reserved != 0) return InvalidArgument;
        if (param->threadParam != nullptr) {
            if (param->threadParam->reserved != 0) return InvalidArgument;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetInputReport(std::uint64_t deviceHandle, std::uint8_t reportId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidSetOutputReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidDebugGetVersion() {
    return 0x1e;
}

int APS5_VABI sceBluetoothHidDisconnectDevice(std::int32_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetDeviceInfo(std::int32_t deviceId, BluetoothHidDeviceInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidGetReportDescriptor(std::uint64_t deviceHandle, void* descriptor, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidInterruptOutput(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidParamInitialize(BluetoothHidParam* param, const BluetoothHidThreadParam* threadParam) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidSetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBluetoothHidThreadParamInitialize(BluetoothHidThreadParam* param, std::int32_t priority, std::uint64_t affinityMask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
