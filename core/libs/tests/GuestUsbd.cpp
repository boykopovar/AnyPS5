#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceUsbd/include/Usbd.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(UsbdDevice*** list);
void APS5_VABI sceUsbdFreeDeviceList(UsbdDevice** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(UsbdTimeval* timeout);
std::int32_t APS5_VABI sceUsbdHandleEvents();
std::int32_t APS5_VABI sceUsbdOpen(UsbdDevice* device, UsbdDeviceHandle** handle);
void APS5_VABI sceUsbdClose(UsbdDeviceHandle* handle);
UsbdDeviceHandle* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(UsbdDevice* device, UsbdDeviceDescriptor* descriptor);
std::int32_t APS5_VABI sceUsbdGetDeviceAddress(UsbdDevice* device);
UsbdSpeed APS5_VABI sceUsbdGetDeviceSpeed(UsbdDevice* device);
std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(UsbdDevice* device, std::uint8_t endpoint);
UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isochronousPacketCount);
void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer);
void APS5_VABI sceUsbdFillControlSetup(unsigned char* buffer, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length);
UsbdControlSetup* APS5_VABI sceUsbdControlTransferGetSetup(UsbdTransfer* transfer);
unsigned char* APS5_VABI sceUsbdControlTransferGetData(UsbdTransfer* transfer);
void APS5_VABI sceUsbdSetIsoPacketLengths(UsbdTransfer* transfer, std::uint32_t length);
unsigned char* APS5_VABI sceUsbdGetIsoPacketBuffer(UsbdTransfer* transfer, std::uint32_t packet);
void APS5_VABI sceUsbdFillControlTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, unsigned char* buffer, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillBulkTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
void APS5_VABI sceUsbdFillIsoTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, std::int32_t isochronousPacketCount, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

bool ThrowsRuntimeError(auto&& call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TransferCallback(UsbdTransfer*) {
}

void CheckEnumeratedDevices(UsbdDevice** list) {
    std::size_t count = 0;
    for (; list[count] != nullptr; ++count) {
        UsbdDeviceDescriptor descriptor{};
        Require(sceUsbdGetDeviceDescriptor(list[count], &descriptor) == USBD_OK, "a device descriptor was not read");
        Require(descriptor.length == 18 && descriptor.descriptorType == USBD_DESCRIPTOR_TYPE_DEVICE, "the device descriptor is not a device descriptor");
        Require(descriptor.configurationCount > 0, "the device descriptor declares no configuration");
        const auto speed = sceUsbdGetDeviceSpeed(list[count]);
        Require(speed >= UsbdSpeed::Unknown && speed <= UsbdSpeed::SuperPlusX2, "the device speed is out of range");
        Require(sceUsbdGetDeviceAddress(list[count]) != 0, "the device address is zero");
        Require(sceUsbdGetMaxPacketSize(list[count], 0) == USBD_ERROR_NOT_FOUND, "the control endpoint packet size was accepted");
    }
    if (count == 0) return;
    UsbdDeviceHandle* handle = nullptr;
    const auto openResult = sceUsbdOpen(list[0], &handle);
    Require(openResult == USBD_OK ? handle != nullptr : handle == nullptr, "the device handle does not match the open result");
    if (handle != nullptr) sceUsbdClose(handle);
}

void CheckTransfer() {
    UsbdTransfer* transfer = sceUsbdAllocTransfer(2);
    Require(transfer != nullptr, "a transfer was not allocated");
    Require(transfer->deviceHandle == nullptr && transfer->buffer == nullptr && transfer->length == 0, "a new transfer is not empty");

    int userData = 0;
    UsbdDeviceHandle* placeholderHandle = reinterpret_cast<UsbdDeviceHandle*>(&userData);
    unsigned char controlBuffer[8 + 4] = {};
    sceUsbdFillControlSetup(controlBuffer, USBD_REQUEST_TYPE_DIRECTION_IN | USBD_REQUEST_TYPE_TYPE_STANDARD | USBD_REQUEST_TYPE_RECIPIENT_DEVICE, USBD_REQUEST_GET_DESCRIPTOR, static_cast<std::uint16_t>(USBD_DESCRIPTOR_TYPE_DEVICE << 8), 0, 4);
    Require(controlBuffer[0] == 0x80 && controlBuffer[1] == USBD_REQUEST_GET_DESCRIPTOR, "the control setup request was not written");
    Require(controlBuffer[2] == 0 && controlBuffer[3] == USBD_DESCRIPTOR_TYPE_DEVICE, "the control setup value was not written");
    Require(controlBuffer[4] == 0 && controlBuffer[5] == 0, "the control setup index was not written");
    Require(controlBuffer[6] == 4 && controlBuffer[7] == 0, "the control setup length was not written");
    sceUsbdFillControlTransfer(transfer, placeholderHandle, controlBuffer, &TransferCallback, &userData, 0);
    const UsbdControlSetup* setup = sceUsbdControlTransferGetSetup(transfer);
    Require(setup->requestType == 0x80 && setup->length == 4, "the control setup was not read back");
    Require(sceUsbdControlTransferGetData(transfer) == controlBuffer + sizeof(UsbdControlSetup), "the control data does not follow the setup");
    Require(transfer->callback == &TransferCallback && transfer->userData == &userData, "the control transfer callback was not kept");
    Require(transfer->deviceHandle == placeholderHandle, "the control transfer handle was not kept");

    unsigned char bulkBuffer[16] = {};
    sceUsbdFillBulkTransfer(transfer, placeholderHandle, USBD_ENDPOINT_IN | 1, bulkBuffer, sizeof(bulkBuffer), &TransferCallback, &userData, 100);
    Require(transfer->endpoint == (USBD_ENDPOINT_IN | 1) && transfer->buffer == bulkBuffer && transfer->length == static_cast<int>(sizeof(bulkBuffer)), "the bulk transfer was not filled");
    Require(transfer->type == static_cast<std::uint8_t>(UsbdTransferType::Bulk), "the bulk transfer type is wrong");
    Require(transfer->callback == &TransferCallback && transfer->userData == &userData, "the bulk transfer callback was not kept");

    sceUsbdFillInterruptTransfer(transfer, placeholderHandle, USBD_ENDPOINT_IN | 2, bulkBuffer, 4, &TransferCallback, &userData, 100);
    Require(transfer->type == static_cast<std::uint8_t>(UsbdTransferType::Interrupt) && transfer->length == 4, "the interrupt transfer was not filled");
    Require(transfer->callback == &TransferCallback && transfer->userData == &userData, "the interrupt transfer callback was not kept");

    sceUsbdFillIsoTransfer(transfer, placeholderHandle, USBD_ENDPOINT_IN | 3, bulkBuffer, 16, 2, &TransferCallback, &userData, 100);
    Require(transfer->type == static_cast<std::uint8_t>(UsbdTransferType::Isochronous) && transfer->isochronousPacketCount == 2, "the isochronous transfer was not filled");
    Require(transfer->callback == &TransferCallback && transfer->userData == &userData, "the isochronous transfer callback was not kept");
    sceUsbdSetIsoPacketLengths(transfer, 8);
    Require(sceUsbdGetIsoPacketBuffer(transfer, 0) == bulkBuffer, "the first isochronous packet buffer is wrong");
    Require(sceUsbdGetIsoPacketBuffer(transfer, 1) == bulkBuffer + 8, "the second isochronous packet buffer is wrong");
    Require(ThrowsRuntimeError([transfer] { sceUsbdGetIsoPacketBuffer(transfer, 2); }), "an out-of-range isochronous packet was accepted");

    sceUsbdFreeTransfer(transfer);
}

}

int main() {
    static_assert(sizeof(UsbdControlSetup) == 8);

    Require(sceUsbdInit() == USBD_OK, "initialization failed");

    UsbdDevice** list = nullptr;
    const auto deviceCount = sceUsbdGetDeviceList(&list);
    Require(deviceCount >= 0, "device enumeration failed");
    Require(list != nullptr, "the device list is null");
    Require(list[deviceCount] == nullptr, "the device list is not null-terminated at the reported count");
    CheckEnumeratedDevices(list);
    sceUsbdFreeDeviceList(list, 1);
    Require(sceUsbdGetDeviceList(nullptr) == USBD_ERROR_INVALID_ARG, "null list accepted");

    Require(sceUsbdHandleEventsTimeout(nullptr) == USBD_ERROR_INVALID_ARG, "null timeout accepted");
    UsbdTimeval outOfRange{0, 1000000};
    Require(sceUsbdHandleEventsTimeout(&outOfRange) == USBD_ERROR_INVALID_ARG, "out-of-range microseconds accepted");
    UsbdTimeval negative{-1, 0};
    Require(sceUsbdHandleEventsTimeout(&negative) == USBD_ERROR_INVALID_ARG, "negative seconds accepted");
    UsbdTimeval immediate{0, 0};
    Require(sceUsbdHandleEventsTimeout(&immediate) == USBD_OK, "event handling with a zero timeout failed");
    UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsTimeout(&timeout) == USBD_OK, "event handling failed");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");

    Require(ThrowsRuntimeError([] { sceUsbdOpen(nullptr, nullptr); }), "a null device was opened");
    Require(ThrowsRuntimeError([] { sceUsbdClose(nullptr); }), "a null handle was closed");
    Require(ThrowsRuntimeError([] { sceUsbdGetDeviceDescriptor(nullptr, nullptr); }), "a null device descriptor was read");
    Require(ThrowsRuntimeError([] { sceUsbdGetDeviceAddress(nullptr); }), "a null device address was read");
    Require(ThrowsRuntimeError([] { sceUsbdGetDeviceSpeed(nullptr); }), "a null device speed was read");
    Require(ThrowsRuntimeError([] { sceUsbdGetMaxPacketSize(nullptr, 0); }), "a null device packet size was read");
    Require(ThrowsRuntimeError([] { sceUsbdAllocTransfer(-1); }), "a negative isochronous packet count was accepted");
    Require(sceUsbdOpenDeviceWithVidPid(0, 0) == nullptr, "a device with the zero vendor and product id was opened");

    CheckTransfer();

    sceUsbdExit();
    std::puts("USBD tests passed");
    return 0;
}
