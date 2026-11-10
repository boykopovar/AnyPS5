#pragma once

#include "SceTypes.hpp"
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct UsbdIsoPacketDescriptor {
    std::uint32_t length;
    std::uint32_t actualLength;
    std::int32_t status;
};

using UsbdTransferCallback = void (APS5_VABI*)(struct UsbdTransfer*);

struct UsbdTransfer {
    void* deviceHandle;
    std::uint8_t flags;
    std::uint8_t endpoint;
    std::uint8_t type;
    std::uint32_t timeout;
    std::int32_t status;
    std::int32_t length;
    std::int32_t actualLength;
    UsbdTransferCallback callback;
    void* userData;
    std::uint8_t* buffer;
    std::int32_t numIsoPackets;
    UsbdIsoPacketDescriptor isoPacketDescriptors[1];
};

extern "C" {

std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets);
void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillBulkTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillIsoTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, std::int32_t numIsoPackets, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillControlSetup(std::uint8_t* buffer, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length);
void APS5_VABI sceUsbdFillControlTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t* buffer, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
std::uint8_t* APS5_VABI sceUsbdControlTransferGetData(UsbdTransfer* transfer);
std::uint8_t* APS5_VABI sceUsbdControlTransferGetSetup(UsbdTransfer* transfer);
void APS5_VABI sceUsbdSetIsoPacketLengths(UsbdTransfer* transfer, std::uint32_t length);
std::uint8_t* APS5_VABI sceUsbdGetIsoPacketBuffer(UsbdTransfer* transfer, std::uint32_t packet);
std::int32_t APS5_VABI sceUsbdBulkTransfer(void* deviceHandle, std::uint8_t endpoint, std::uint8_t* data, std::int32_t length, std::int32_t* transferred, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdInterruptTransfer(void* deviceHandle, std::uint8_t endpoint, std::uint8_t* data, std::int32_t length, std::int32_t* transferred, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdCancelTransfer(UsbdTransfer* transfer);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdFreeConfigDescriptor(void* config);
std::int32_t APS5_VABI sceUsbdAttachKernelDriver(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdCheckConnected(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdClaimInterface(void* deviceHandle, std::int32_t interfaceNumber);
void APS5_VABI sceUsbdClose(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdControlTransfer(void* deviceHandle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint8_t* data, std::uint16_t length, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdGetActiveConfigDescriptor(void* device, void** config);
std::uint8_t APS5_VABI sceUsbdGetBusNumber(void* device);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config);
std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(void* device);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor);
std::int32_t APS5_VABI sceUsbdGetStringDescriptor(void* deviceHandle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* data, std::int32_t length);
std::int32_t APS5_VABI sceUsbdKernelDriverActive(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdOpen(void* device, void** deviceHandle);
void* APS5_VABI sceUsbdRefDevice(void* device);
std::int32_t APS5_VABI sceUsbdReleaseInterface(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdResetDevice(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdSetConfiguration(void* deviceHandle, std::int32_t configuration);
std::int32_t APS5_VABI sceUsbdSubmitTransfer(UsbdTransfer* transfer);
void APS5_VABI sceUsbdUnrefDevice(void* device);
std::int32_t APS5_VABI sceUsbdClearHalt(void* deviceHandle, std::uint8_t endpoint);
std::int32_t APS5_VABI sceUsbdDetachKernelDriver(void* deviceHandle, int interfaceNumber);
std::int32_t APS5_VABI sceUsbdEventHandlerActive();
std::int32_t APS5_VABI sceUsbdGetConfigDescriptorByValue(void* device, std::uint8_t configurationValue, void** config);
std::int32_t APS5_VABI sceUsbdGetConfiguration(void* deviceHandle, std::int32_t* configuration);
std::int32_t APS5_VABI sceUsbdGetDescriptor(void* deviceHandle, std::uint8_t descriptorType, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length);
void* APS5_VABI sceUsbdGetDevice(void* deviceHandle);
int APS5_VABI sceUsbdGetDeviceSpeed(void* device);
std::int32_t APS5_VABI sceUsbdGetMaxIsoPacketSize(void* device, std::uint8_t endpoint);
std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(void* device, std::uint8_t endpoint);
std::int32_t APS5_VABI sceUsbdGetStringDescriptorAscii(void* deviceHandle, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length);
std::int32_t APS5_VABI sceUsbdHandleEvents();
std::int32_t APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval* timeout);
void APS5_VABI sceUsbdLockEventWaiters();
void APS5_VABI sceUsbdLockEvents();
void* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId);
std::int32_t APS5_VABI sceUsbdSetInterfaceAltSetting(void* deviceHandle, int interfaceNumber, int alternateSetting);
std::int32_t APS5_VABI sceUsbdTryLockEvents();
void APS5_VABI sceUsbdUnlockEventWaiters();
void APS5_VABI sceUsbdUnlockEvents();
std::int32_t APS5_VABI sceUsbdWaitForEvent(UsbdTimeval* timeout);

}
