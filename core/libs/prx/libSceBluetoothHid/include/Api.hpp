#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

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

extern "C" {

std::int32_t APS5_VABI sceBluetoothHidInit();
std::int32_t APS5_VABI sceBluetoothHidRegisterCallback(BluetoothHidCallback callback, void* userData, const BluetoothHidParam* param);
std::int32_t APS5_VABI sceBluetoothHidUnregisterCallback();
std::int32_t APS5_VABI sceBluetoothHidRegisterDevice(std::uint16_t vendorId, std::uint16_t productId);
std::int32_t APS5_VABI sceBluetoothHidUnregisterDevice(std::uint16_t vendorId, std::uint16_t productId);
int APS5_VABI sceBluetoothHidGetDeviceName(std::uint64_t deviceHandle, BluetoothHidDeviceName* name);
int APS5_VABI sceBluetoothHidGetInputReport(std::uint64_t deviceHandle, std::uint8_t reportId);
int APS5_VABI sceBluetoothHidSetOutputReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size);
int APS5_VABI sceBluetoothHidDebugGetVersion();
int APS5_VABI sceBluetoothHidDisconnectDevice(std::int32_t deviceId);
int APS5_VABI sceBluetoothHidGetDeviceInfo(std::int32_t deviceId, BluetoothHidDeviceInfo* info);
int APS5_VABI sceBluetoothHidGetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId);
int APS5_VABI sceBluetoothHidGetReportDescriptor(std::uint64_t deviceHandle, void* descriptor, std::size_t size);
int APS5_VABI sceBluetoothHidInterruptOutput(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size);
int APS5_VABI sceBluetoothHidParamInitialize(BluetoothHidParam* param, const BluetoothHidThreadParam* threadParam);
int APS5_VABI sceBluetoothHidSetFeatureReport(std::uint64_t deviceHandle, std::uint8_t reportId, const void* data, std::size_t size);
int APS5_VABI sceBluetoothHidThreadParamInitialize(BluetoothHidThreadParam* param, std::int32_t priority, std::uint64_t affinityMask);

}
