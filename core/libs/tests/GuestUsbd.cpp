#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <libusb.h>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

using UsbdTransferCallback = void (APS5_VABI *)(libusb_transfer*);

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(libusb_device*** list);
void APS5_VABI sceUsbdFreeDeviceList(libusb_device** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
libusb_device* APS5_VABI sceUsbdRefDevice(libusb_device* device);
void APS5_VABI sceUsbdUnrefDevice(libusb_device* device);
std::uint8_t APS5_VABI sceUsbdGetBusNumber(libusb_device* device);
std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(libusb_device* device);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(libusb_device* device, libusb_device_descriptor* descriptor);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(libusb_device* device, std::uint8_t index, libusb_config_descriptor** config);
std::int32_t APS5_VABI sceUsbdGetActiveConfigDescriptor(libusb_device* device, libusb_config_descriptor** config);
void APS5_VABI sceUsbdFreeConfigDescriptor(libusb_config_descriptor* descriptor);
std::int32_t APS5_VABI sceUsbdOpen(libusb_device* device, libusb_device_handle** handle);
void APS5_VABI sceUsbdClose(libusb_device_handle* handle);
std::int32_t APS5_VABI sceUsbdAttachKernelDriver(libusb_device_handle* handle, int interfaceNumber);
std::int32_t APS5_VABI sceUsbdClaimInterface(libusb_device_handle* handle, int interfaceNumber);
std::int32_t APS5_VABI sceUsbdReleaseInterface(libusb_device_handle* handle, int interfaceNumber);
std::int32_t APS5_VABI sceUsbdKernelDriverActive(libusb_device_handle* handle, int interfaceNumber);
std::int32_t APS5_VABI sceUsbdResetDevice(libusb_device_handle* handle);
std::int32_t APS5_VABI sceUsbdSetConfiguration(libusb_device_handle* handle, int configuration);
libusb_transfer* APS5_VABI sceUsbdAllocTransfer(int isoPackets);
void APS5_VABI sceUsbdFreeTransfer(libusb_transfer* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(libusb_transfer* transfer, libusb_device_handle* handle, std::uint8_t endpoint, unsigned char* buffer, int length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
int APS5_VABI sceUsbdSubmitTransfer(libusb_transfer* transfer);
int APS5_VABI sceUsbdCancelTransfer(libusb_transfer* transfer);
int APS5_VABI sceUsbdCheckConnected(libusb_device_handle* handle);
}

namespace {

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t notFound = static_cast<std::int32_t>(0x80240005);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

template<typename TCall>
void RequireUnsupported(TCall call) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "unsupported operation did not throw");
}

void APS5_VABI TransferCallback(libusb_transfer* transfer) {
    ++*static_cast<int*>(transfer->user_data);
}

void CheckTransferPreparation() {
    Require(sceUsbdAllocTransfer(-1) == nullptr, "negative isochronous count accepted");
    for (const int packets : {0, 3}) {
        auto* transfer = sceUsbdAllocTransfer(packets);
        Require(transfer != nullptr, "transfer allocation failed");
        Require(transfer->flags == 0 && transfer->buffer == nullptr && transfer->length == 0, "transfer was not initialized");
        unsigned char buffer[16]{};
        int callbacks = 0;
        sceUsbdFillInterruptTransfer(transfer, nullptr, 0x81, buffer, sizeof(buffer), TransferCallback, &callbacks, 250);
        Require(transfer->type == LIBUSB_TRANSFER_TYPE_INTERRUPT && transfer->endpoint == 0x81, "interrupt endpoint not preserved");
        Require(transfer->buffer == buffer && transfer->length == sizeof(buffer) && transfer->timeout == 250, "transfer buffer or timeout not preserved");
        Require(transfer->user_data == &callbacks, "callback user data not preserved");
        auto callback = reinterpret_cast<UsbdTransferCallback>(transfer->callback);
        Require(callback == TransferCallback, "guest callback pointer not preserved");
        callback(transfer);
        Require(callbacks == 1, "guest callback ABI or user data incorrect");
        sceUsbdFreeTransfer(transfer);
    }
    sceUsbdFreeTransfer(nullptr);
}

void CheckDevices() {
    libusb_device** list = nullptr;
    const auto count = sceUsbdGetDeviceList(&list);
    Require(count >= 0 && list != nullptr, "device enumeration failed");
    Require(list[count] == nullptr, "device list is not null-terminated");
    for (std::int64_t i = 0; i < count; ++i) {
        auto* device = list[i];
        Require(device != nullptr, "null entry before device list terminator");
        Require(sceUsbdRefDevice(device) == device, "ref changed the device pointer");
        sceUsbdUnrefDevice(device);
        libusb_device_descriptor descriptor{};
        Require(sceUsbdGetDeviceDescriptor(device, &descriptor) == 0, "cached device descriptor unavailable");
        Require(descriptor.bLength == LIBUSB_DT_DEVICE_SIZE && descriptor.bDescriptorType == LIBUSB_DT_DEVICE, "invalid device descriptor");
        Require(sceUsbdGetDeviceAddress(device) <= 127, "invalid USB device address");
        libusb_config_descriptor* config = nullptr;
        Require(sceUsbdGetConfigDescriptor(device, descriptor.bNumConfigurations, &config) == notFound, "missing config did not map to NOT_FOUND");
        Require(config == nullptr, "missing config returned a descriptor");
        if (descriptor.bNumConfigurations != 0 && sceUsbdGetConfigDescriptor(device, 0, &config) == 0) {
            Require(config != nullptr && config->bDescriptorType == LIBUSB_DT_CONFIG, "invalid config descriptor");
            sceUsbdFreeConfigDescriptor(config);
        }
        config = nullptr;
        if (sceUsbdGetActiveConfigDescriptor(device, &config) == 0) {
            Require(config != nullptr && config->bDescriptorType == LIBUSB_DT_CONFIG, "invalid active config descriptor");
            sceUsbdFreeConfigDescriptor(config);
        }
    }
    sceUsbdFreeDeviceList(list, 1);
    list = nullptr;
    const auto retainedCount = sceUsbdGetDeviceList(&list);
    Require(retainedCount >= 0 && list != nullptr, "second enumeration failed");
    const std::vector<libusb_device*> retained(list, list + retainedCount);
    sceUsbdFreeDeviceList(list, 0);
    for (auto* device : retained) {
        libusb_device_descriptor descriptor{};
        Require(sceUsbdGetDeviceDescriptor(device, &descriptor) == 0, "device reference was not retained");
        sceUsbdUnrefDevice(device);
    }
    std::printf("USBD: enumerated %lld host devices\n", static_cast<long long>(count));
}

void CheckInvalidArguments() {
    Require(sceUsbdGetDeviceList(nullptr) == invalidArgument, "null list accepted");
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalidArgument, "null timeout accepted");
    for (const UsbdTimeval invalid : {UsbdTimeval{-1, 0}, UsbdTimeval{0, -1}, UsbdTimeval{0, 1000000}}) {
        Require(sceUsbdHandleEventsTimeout(&invalid) == invalidArgument, "invalid timeout accepted");
    }
    libusb_device_descriptor descriptor{};
    libusb_config_descriptor* config = nullptr;
    libusb_device_handle* handle = nullptr;
    Require(sceUsbdGetDeviceDescriptor(nullptr, &descriptor) == invalidArgument, "null device accepted for descriptor");
    Require(sceUsbdGetConfigDescriptor(nullptr, 0, &config) == invalidArgument, "null device accepted for config");
    Require(sceUsbdGetActiveConfigDescriptor(nullptr, &config) == invalidArgument, "null device accepted for active config");
    Require(sceUsbdOpen(nullptr, &handle) == invalidArgument, "null device opened");
    Require(sceUsbdAttachKernelDriver(nullptr, 0) == invalidArgument, "null handle accepted for attach");
    Require(sceUsbdClaimInterface(nullptr, 0) == invalidArgument, "null handle accepted for claim");
    Require(sceUsbdReleaseInterface(nullptr, 0) == invalidArgument, "null handle accepted for release");
    Require(sceUsbdKernelDriverActive(nullptr, 0) == invalidArgument, "null handle accepted for driver query");
    Require(sceUsbdResetDevice(nullptr) == invalidArgument, "null handle accepted for reset");
    Require(sceUsbdSetConfiguration(nullptr, 0) == invalidArgument, "null handle accepted for configuration");
    Require(sceUsbdRefDevice(nullptr) == nullptr, "null device ref returned a pointer");
    sceUsbdUnrefDevice(nullptr);
    sceUsbdClose(nullptr);
    sceUsbdFreeConfigDescriptor(nullptr);
    sceUsbdFreeDeviceList(nullptr, 1);
    Require(sceUsbdSubmitTransfer(nullptr) == invalidArgument, "null submit transfer accepted");
    Require(sceUsbdCancelTransfer(nullptr) == invalidArgument, "null cancel transfer accepted");
    RequireUnsupported([] { sceUsbdCheckConnected(nullptr); });
}

}

int main() {
    Require(sceUsbdInit() == 0, "initialization failed");
    Require(sceUsbdInit() == 0, "repeated initialization failed");
    sceUsbdExit();
    CheckDevices();
    CheckTransferPreparation();
    CheckInvalidArguments();
    const UsbdTimeval timeout{0, 50000};
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(sceUsbdEventHandlingOk() == 1, "event handling unavailable");
    sceUsbdExit();
    RequireUnsupported([] { sceUsbdEventHandlingOk(); });
    Require(sceUsbdInit() == 0, "reinitialization failed");
    libusb_device** list = nullptr;
    Require(sceUsbdGetDeviceList(&list) >= 0, "enumeration after reinitialization failed");
    sceUsbdFreeDeviceList(list, 1);
    sceUsbdExit();
    std::puts("USBD tests passed");
    return 0;
}
