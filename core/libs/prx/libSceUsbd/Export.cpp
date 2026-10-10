#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUsbd/include/Usbd.hpp"
#include "prx/libSceUsbd/include/UsbdTransfer.hpp"

#include <libusb.h>

static_assert(sizeof(UsbdIsoPacketDescriptor) == sizeof(libusb_iso_packet_descriptor));
static_assert(offsetof(UsbdIsoPacketDescriptor, length) == offsetof(libusb_iso_packet_descriptor, length));
static_assert(offsetof(UsbdIsoPacketDescriptor, actualLength) == offsetof(libusb_iso_packet_descriptor, actual_length));
static_assert(offsetof(UsbdIsoPacketDescriptor, status) == offsetof(libusb_iso_packet_descriptor, status));

static_assert(sizeof(UsbdTransfer) == sizeof(libusb_transfer));
static_assert(offsetof(UsbdTransfer, deviceHandle) == offsetof(libusb_transfer, dev_handle));
static_assert(offsetof(UsbdTransfer, flags) == offsetof(libusb_transfer, flags));
static_assert(offsetof(UsbdTransfer, endpoint) == offsetof(libusb_transfer, endpoint));
static_assert(offsetof(UsbdTransfer, type) == offsetof(libusb_transfer, type));
static_assert(offsetof(UsbdTransfer, timeout) == offsetof(libusb_transfer, timeout));
static_assert(offsetof(UsbdTransfer, status) == offsetof(libusb_transfer, status));
static_assert(offsetof(UsbdTransfer, length) == offsetof(libusb_transfer, length));
static_assert(offsetof(UsbdTransfer, actualLength) == offsetof(libusb_transfer, actual_length));
static_assert(offsetof(UsbdTransfer, callback) == offsetof(libusb_transfer, callback));
static_assert(offsetof(UsbdTransfer, userData) == offsetof(libusb_transfer, user_data));
static_assert(offsetof(UsbdTransfer, buffer) == offsetof(libusb_transfer, buffer));
static_assert(offsetof(UsbdTransfer, isochronousPacketCount) == offsetof(libusb_transfer, num_iso_packets));
static_assert(offsetof(UsbdTransfer, isochronousPackets) == offsetof(libusb_transfer, iso_packet_desc));

static_assert(sizeof(UsbdDeviceDescriptor) == sizeof(libusb_device_descriptor));
static_assert(offsetof(UsbdDeviceDescriptor, vendorId) == offsetof(libusb_device_descriptor, idVendor));
static_assert(offsetof(UsbdDeviceDescriptor, productId) == offsetof(libusb_device_descriptor, idProduct));
static_assert(offsetof(UsbdDeviceDescriptor, configurationCount) == offsetof(libusb_device_descriptor, bNumConfigurations));

static_assert(sizeof(UsbdEndpointDescriptor) == sizeof(libusb_endpoint_descriptor));
static_assert(offsetof(UsbdEndpointDescriptor, endpointAddress) == offsetof(libusb_endpoint_descriptor, bEndpointAddress));
static_assert(offsetof(UsbdEndpointDescriptor, extraLength) == offsetof(libusb_endpoint_descriptor, extra_length));

static_assert(sizeof(UsbdInterfaceDescriptor) == sizeof(libusb_interface_descriptor));
static_assert(offsetof(UsbdInterfaceDescriptor, endpoints) == offsetof(libusb_interface_descriptor, endpoint));
static_assert(offsetof(UsbdInterfaceDescriptor, interfaceNumber) == offsetof(libusb_interface_descriptor, bInterfaceNumber));

static_assert(sizeof(UsbdInterface) == sizeof(libusb_interface));
static_assert(offsetof(UsbdInterface, alternateSettings) == offsetof(libusb_interface, altsetting));
static_assert(offsetof(UsbdInterface, alternateSettingCount) == offsetof(libusb_interface, num_altsetting));

static_assert(sizeof(UsbdConfigDescriptor) == sizeof(libusb_config_descriptor));
static_assert(offsetof(UsbdConfigDescriptor, interfaces) == offsetof(libusb_config_descriptor, interface));
static_assert(offsetof(UsbdConfigDescriptor, totalLength) == offsetof(libusb_config_descriptor, wTotalLength));
static_assert(offsetof(UsbdConfigDescriptor, extraLength) == offsetof(libusb_config_descriptor, extra_length));

static_assert(USBD_TRANSFER_SHORT_NOT_OK == LIBUSB_TRANSFER_SHORT_NOT_OK);
static_assert(USBD_TRANSFER_FREE_BUFFER == LIBUSB_TRANSFER_FREE_BUFFER);
static_assert(USBD_TRANSFER_FREE_TRANSFER == LIBUSB_TRANSFER_FREE_TRANSFER);
static_assert(USBD_TRANSFER_ADD_ZERO_PACKET == LIBUSB_TRANSFER_ADD_ZERO_PACKET);

static_assert(USBD_ERROR_IO == static_cast<int>(0x80240000) - LIBUSB_ERROR_IO);
static_assert(USBD_ERROR_INVALID_ARG == static_cast<int>(0x80240000) - LIBUSB_ERROR_INVALID_PARAM);
static_assert(USBD_ERROR_ACCESS == static_cast<int>(0x80240000) - LIBUSB_ERROR_ACCESS);
static_assert(USBD_ERROR_NO_DEVICE == static_cast<int>(0x80240000) - LIBUSB_ERROR_NO_DEVICE);
static_assert(USBD_ERROR_NOT_FOUND == static_cast<int>(0x80240000) - LIBUSB_ERROR_NOT_FOUND);
static_assert(USBD_ERROR_BUSY == static_cast<int>(0x80240000) - LIBUSB_ERROR_BUSY);
static_assert(USBD_ERROR_TIMEOUT == static_cast<int>(0x80240000) - LIBUSB_ERROR_TIMEOUT);
static_assert(USBD_ERROR_OVERFLOW == static_cast<int>(0x80240000) - LIBUSB_ERROR_OVERFLOW);
static_assert(USBD_ERROR_PIPE == static_cast<int>(0x80240000) - LIBUSB_ERROR_PIPE);
static_assert(USBD_ERROR_INTERRUPTED == static_cast<int>(0x80240000) - LIBUSB_ERROR_INTERRUPTED);
static_assert(USBD_ERROR_NO_MEMORY == static_cast<int>(0x80240000) - LIBUSB_ERROR_NO_MEM);
static_assert(USBD_ERROR_NOT_SUPPORTED == static_cast<int>(0x80240000) - LIBUSB_ERROR_NOT_SUPPORTED);
static_assert(USBD_ERROR_OTHER == static_cast<int>(0x802400FF));

static_assert(static_cast<int>(UsbdTransferStatus::Completed) == LIBUSB_TRANSFER_COMPLETED);
static_assert(static_cast<int>(UsbdTransferStatus::Error) == LIBUSB_TRANSFER_ERROR);
static_assert(static_cast<int>(UsbdTransferStatus::TimedOut) == LIBUSB_TRANSFER_TIMED_OUT);
static_assert(static_cast<int>(UsbdTransferStatus::Cancelled) == LIBUSB_TRANSFER_CANCELLED);
static_assert(static_cast<int>(UsbdTransferStatus::Stall) == LIBUSB_TRANSFER_STALL);
static_assert(static_cast<int>(UsbdTransferStatus::NoDevice) == LIBUSB_TRANSFER_NO_DEVICE);
static_assert(static_cast<int>(UsbdTransferStatus::Overflow) == LIBUSB_TRANSFER_OVERFLOW);

static_assert(static_cast<int>(UsbdTransferType::Control) == LIBUSB_TRANSFER_TYPE_CONTROL);
static_assert(static_cast<int>(UsbdTransferType::Isochronous) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS);
static_assert(static_cast<int>(UsbdTransferType::Bulk) == LIBUSB_TRANSFER_TYPE_BULK);
static_assert(static_cast<int>(UsbdTransferType::Interrupt) == LIBUSB_TRANSFER_TYPE_INTERRUPT);

static_assert(static_cast<int>(UsbdSpeed::Unknown) == LIBUSB_SPEED_UNKNOWN);
static_assert(static_cast<int>(UsbdSpeed::Low) == LIBUSB_SPEED_LOW);
static_assert(static_cast<int>(UsbdSpeed::Full) == LIBUSB_SPEED_FULL);
static_assert(static_cast<int>(UsbdSpeed::High) == LIBUSB_SPEED_HIGH);
static_assert(static_cast<int>(UsbdSpeed::Super) == LIBUSB_SPEED_SUPER);
static_assert(static_cast<int>(UsbdSpeed::SuperPlus) == LIBUSB_SPEED_SUPER_PLUS);
static_assert(static_cast<int>(UsbdSpeed::SuperPlusX2) == LIBUSB_SPEED_SUPER_PLUS_X2);

static_assert(static_cast<int>(UsbdLogLevel::None) == LIBUSB_LOG_LEVEL_NONE);
static_assert(static_cast<int>(UsbdLogLevel::Error) == LIBUSB_LOG_LEVEL_ERROR);
static_assert(static_cast<int>(UsbdLogLevel::Warning) == LIBUSB_LOG_LEVEL_WARNING);
static_assert(static_cast<int>(UsbdLogLevel::Info) == LIBUSB_LOG_LEVEL_INFO);
static_assert(static_cast<int>(UsbdLogLevel::Debug) == LIBUSB_LOG_LEVEL_DEBUG);

static_assert(USBD_ENDPOINT_IN == LIBUSB_ENDPOINT_IN);
static_assert(USBD_ENDPOINT_OUT == LIBUSB_ENDPOINT_OUT);
static_assert(USBD_TRANSFER_TYPE_MASK == LIBUSB_TRANSFER_TYPE_MASK);
static_assert(USBD_ENDPOINT_NUMBER_MASK == LIBUSB_ENDPOINT_ADDRESS_MASK);
static_assert(USBD_ENDPOINT_IN == LIBUSB_ENDPOINT_DIR_MASK);
static_assert(USBD_REQUEST_TYPE_DIRECTION_IN == LIBUSB_ENDPOINT_IN);
static_assert(USBD_REQUEST_TYPE_DIRECTION_OUT == LIBUSB_ENDPOINT_OUT);
static_assert(USBD_REQUEST_TYPE_TYPE_STANDARD == LIBUSB_REQUEST_TYPE_STANDARD);
static_assert(USBD_REQUEST_TYPE_TYPE_CLASS == LIBUSB_REQUEST_TYPE_CLASS);
static_assert(USBD_REQUEST_TYPE_TYPE_VENDOR == LIBUSB_REQUEST_TYPE_VENDOR);
static_assert(USBD_REQUEST_TYPE_RECIPIENT_DEVICE == LIBUSB_RECIPIENT_DEVICE);
static_assert(USBD_REQUEST_TYPE_RECIPIENT_INTERFACE == LIBUSB_RECIPIENT_INTERFACE);
static_assert(USBD_REQUEST_TYPE_RECIPIENT_ENDPOINT == LIBUSB_RECIPIENT_ENDPOINT);
static_assert(USBD_REQUEST_TYPE_RECIPIENT_OTHER == LIBUSB_RECIPIENT_OTHER);
static_assert(USBD_REQUEST_GET_STATUS == LIBUSB_REQUEST_GET_STATUS);
static_assert(USBD_REQUEST_CLEAR_FEATURE == LIBUSB_REQUEST_CLEAR_FEATURE);
static_assert(USBD_REQUEST_SET_FEATURE == LIBUSB_REQUEST_SET_FEATURE);
static_assert(USBD_REQUEST_GET_DESCRIPTOR == LIBUSB_REQUEST_GET_DESCRIPTOR);
static_assert(USBD_REQUEST_SET_DESCRIPTOR == LIBUSB_REQUEST_SET_DESCRIPTOR);
static_assert(USBD_REQUEST_GET_CONFIGURATION == LIBUSB_REQUEST_GET_CONFIGURATION);
static_assert(USBD_REQUEST_SET_CONFIGURATION == LIBUSB_REQUEST_SET_CONFIGURATION);
static_assert(USBD_DESCRIPTOR_TYPE_DEVICE == LIBUSB_DT_DEVICE);
static_assert(USBD_DESCRIPTOR_TYPE_CONFIGURATION == LIBUSB_DT_CONFIG);
static_assert(USBD_DESCRIPTOR_TYPE_STRING == LIBUSB_DT_STRING);
static_assert(USBD_DESCRIPTOR_TYPE_INTERFACE == LIBUSB_DT_INTERFACE);
static_assert(USBD_DESCRIPTOR_TYPE_ENDPOINT == LIBUSB_DT_ENDPOINT);
static_assert(USBD_DESCRIPTOR_TYPE_BOS == LIBUSB_DT_BOS);

namespace {

constexpr std::size_t kControlSetupSize = sizeof(UsbdControlSetup);

libusb_context* HostContext() {
    return nullptr;
}

libusb_device* HostDevice(UsbdDevice* device) {
    return reinterpret_cast<libusb_device*>(device);
}

libusb_device_handle* HostHandle(UsbdDeviceHandle* handle) {
    return reinterpret_cast<libusb_device_handle*>(handle);
}

libusb_transfer* HostTransfer(UsbdTransfer* transfer) {
    return reinterpret_cast<libusb_transfer*>(transfer);
}

int ToHostError(int result) {
    switch (result) {
        case LIBUSB_SUCCESS: return USBD_OK;
        case LIBUSB_ERROR_IO: return USBD_ERROR_IO;
        case LIBUSB_ERROR_INVALID_PARAM: return USBD_ERROR_INVALID_ARG;
        case LIBUSB_ERROR_ACCESS: return USBD_ERROR_ACCESS;
        case LIBUSB_ERROR_NO_DEVICE: return USBD_ERROR_NO_DEVICE;
        case LIBUSB_ERROR_NOT_FOUND: return USBD_ERROR_NOT_FOUND;
        case LIBUSB_ERROR_BUSY: return USBD_ERROR_BUSY;
        case LIBUSB_ERROR_TIMEOUT: return USBD_ERROR_TIMEOUT;
        case LIBUSB_ERROR_OVERFLOW: return USBD_ERROR_OVERFLOW;
        case LIBUSB_ERROR_PIPE: return USBD_ERROR_PIPE;
        case LIBUSB_ERROR_INTERRUPTED: return USBD_ERROR_INTERRUPTED;
        case LIBUSB_ERROR_NO_MEM: return USBD_ERROR_NO_MEMORY;
        case LIBUSB_ERROR_NOT_SUPPORTED: return USBD_ERROR_NOT_SUPPORTED;
        case LIBUSB_ERROR_OTHER: return USBD_ERROR_OTHER;
        default: return USBD_ERROR_OTHER;
    }
}

void RequireDevice(const UsbdDevice* device) {
    if (device == nullptr) throw std::runtime_error(std::string(__func__) + ": device is null");
}

void RequireHandle(const UsbdDeviceHandle* handle) {
    if (handle == nullptr) throw std::runtime_error(std::string(__func__) + ": device handle is null");
}

void RequireTransfer(const UsbdTransfer* transfer) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": transfer is null");
}

void RequireDescriptor(const void* descriptor) {
    if (descriptor == nullptr) throw std::runtime_error(std::string(__func__) + ": descriptor is null");
}

void RequireBuffer(const unsigned char* buffer, int length) {
    if (buffer == nullptr && length != 0) throw std::runtime_error(std::string(__func__) + ": transfer buffer is null");
    if (length < 0) throw std::runtime_error(std::string(__func__) + ": transfer length is negative");
}

}

extern "C" {

std::int32_t APS5_VABI sceUsbdInit() {
    return ToHostError(libusb_init(HostContext() == nullptr ? nullptr : nullptr));
}

void APS5_VABI sceUsbdExit() {
    libusb_exit(HostContext());
}

std::int64_t APS5_VABI sceUsbdGetDeviceList(UsbdDevice*** list) {
    if (list == nullptr) return USBD_ERROR_INVALID_ARG;
    return static_cast<std::int64_t>(libusb_get_device_list(HostContext(), reinterpret_cast<libusb_device***>(list)));
}

void APS5_VABI sceUsbdFreeDeviceList(UsbdDevice** list, std::int32_t unrefDevices) {
    if (list == nullptr) throw std::runtime_error(std::string(__func__) + ": device list is null");
    libusb_free_device_list(reinterpret_cast<libusb_device**>(list), unrefDevices);
}

UsbdDevice* APS5_VABI sceUsbdRefDevice(UsbdDevice* device) {
    RequireDevice(device);
    return reinterpret_cast<UsbdDevice*>(libusb_ref_device(HostDevice(device)));
}

void APS5_VABI sceUsbdUnrefDevice(UsbdDevice* device) {
    RequireDevice(device);
    libusb_unref_device(HostDevice(device));
}

std::int32_t APS5_VABI sceUsbdGetConfiguration(UsbdDeviceHandle* handle, std::int32_t* configuration) {
    RequireHandle(handle);
    if (configuration == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_get_configuration(HostHandle(handle), configuration));
}

std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(UsbdDevice* device, UsbdDeviceDescriptor* descriptor) {
    RequireDevice(device);
    if (descriptor == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_get_device_descriptor(HostDevice(device), reinterpret_cast<libusb_device_descriptor*>(descriptor)));
}

std::int32_t APS5_VABI sceUsbdGetActiveConfigDescriptor(UsbdDevice* device, UsbdConfigDescriptor** descriptor) {
    RequireDevice(device);
    if (descriptor == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_get_active_config_descriptor(HostDevice(device), reinterpret_cast<libusb_config_descriptor**>(descriptor)));
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(UsbdDevice* device, std::uint8_t index, UsbdConfigDescriptor** descriptor) {
    RequireDevice(device);
    if (descriptor == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_get_config_descriptor(HostDevice(device), index, reinterpret_cast<libusb_config_descriptor**>(descriptor)));
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptorByValue(UsbdDevice* device, std::uint8_t value, UsbdConfigDescriptor** descriptor) {
    RequireDevice(device);
    if (descriptor == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_get_config_descriptor_by_value(HostDevice(device), value, reinterpret_cast<libusb_config_descriptor**>(descriptor)));
}

void APS5_VABI sceUsbdFreeConfigDescriptor(UsbdConfigDescriptor* descriptor) {
    RequireDescriptor(descriptor);
    libusb_free_config_descriptor(reinterpret_cast<libusb_config_descriptor*>(descriptor));
}

std::uint8_t APS5_VABI sceUsbdGetBusNumber(UsbdDevice* device) {
    RequireDevice(device);
    return libusb_get_bus_number(HostDevice(device));
}

std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(UsbdDevice* device) {
    RequireDevice(device);
    return libusb_get_device_address(HostDevice(device));
}

UsbdSpeed APS5_VABI sceUsbdGetDeviceSpeed(UsbdDevice* device) {
    RequireDevice(device);
    return static_cast<UsbdSpeed>(libusb_get_device_speed(HostDevice(device)));
}

std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(UsbdDevice* device, std::uint8_t endpoint) {
    RequireDevice(device);
    return ToHostError(libusb_get_max_packet_size(HostDevice(device), endpoint));
}

std::int32_t APS5_VABI sceUsbdGetMaxIsoPacketSize(UsbdDevice* device, std::uint8_t endpoint) {
    RequireDevice(device);
    return ToHostError(libusb_get_max_iso_packet_size(HostDevice(device), endpoint));
}

std::int32_t APS5_VABI sceUsbdOpen(UsbdDevice* device, UsbdDeviceHandle** handle) {
    RequireDevice(device);
    if (handle == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_open(HostDevice(device), reinterpret_cast<libusb_device_handle**>(handle)));
}

void APS5_VABI sceUsbdClose(UsbdDeviceHandle* handle) {
    RequireHandle(handle);
    libusb_close(HostHandle(handle));
}

UsbdDevice* APS5_VABI sceUsbdGetDevice(UsbdDeviceHandle* handle) {
    RequireHandle(handle);
    return reinterpret_cast<UsbdDevice*>(libusb_get_device(HostHandle(handle)));
}

UsbdDeviceHandle* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId) {
    libusb_device** devices = nullptr;
    const auto count = libusb_get_device_list(HostContext(), &devices);
    if (count < 0) return nullptr;
    libusb_device_handle* handle = nullptr;
    for (std::int64_t index = 0; index < count; ++index) {
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(devices[index], &descriptor) != LIBUSB_SUCCESS) continue;
        if (descriptor.idVendor != vendorId || descriptor.idProduct != productId) continue;
        if (libusb_open(devices[index], &handle) == LIBUSB_SUCCESS) break;
        handle = nullptr;
    }
    libusb_free_device_list(devices, 1);
    return reinterpret_cast<UsbdDeviceHandle*>(handle);
}

std::int32_t APS5_VABI sceUsbdSetConfiguration(UsbdDeviceHandle* handle, std::int32_t configuration) {
    RequireHandle(handle);
    return ToHostError(libusb_set_configuration(HostHandle(handle), configuration));
}

std::int32_t APS5_VABI sceUsbdClaimInterface(UsbdDeviceHandle* handle, std::int32_t interfaceNumber) {
    RequireHandle(handle);
    const auto active = libusb_kernel_driver_active(HostHandle(handle), interfaceNumber);
    if (active == 1) {
        const auto detached = libusb_detach_kernel_driver(HostHandle(handle), interfaceNumber);
        if (detached != LIBUSB_SUCCESS) return ToHostError(detached);
    }
    return ToHostError(libusb_claim_interface(HostHandle(handle), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdReleaseInterface(UsbdDeviceHandle* handle, std::int32_t interfaceNumber) {
    RequireHandle(handle);
    return ToHostError(libusb_release_interface(HostHandle(handle), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdSetInterfaceAltSetting(UsbdDeviceHandle* handle, std::int32_t interfaceNumber, std::int32_t alternateSetting) {
    RequireHandle(handle);
    return ToHostError(libusb_set_interface_alt_setting(HostHandle(handle), interfaceNumber, alternateSetting));
}

std::int32_t APS5_VABI sceUsbdClearHalt(UsbdDeviceHandle* handle, std::uint8_t endpoint) {
    RequireHandle(handle);
    return ToHostError(libusb_clear_halt(HostHandle(handle), endpoint));
}

std::int32_t APS5_VABI sceUsbdResetDevice(UsbdDeviceHandle* handle) {
    RequireHandle(handle);
    return ToHostError(libusb_reset_device(HostHandle(handle)));
}

std::int32_t APS5_VABI sceUsbdKernelDriverActive(UsbdDeviceHandle* handle, std::int32_t interfaceNumber) {
    RequireHandle(handle);
    return ToHostError(libusb_kernel_driver_active(HostHandle(handle), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdDetachKernelDriver(UsbdDeviceHandle* handle, std::int32_t interfaceNumber) {
    RequireHandle(handle);
    return ToHostError(libusb_detach_kernel_driver(HostHandle(handle), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdAttachKernelDriver(UsbdDeviceHandle* handle, std::int32_t interfaceNumber) {
    RequireHandle(handle);
    return ToHostError(libusb_attach_kernel_driver(HostHandle(handle), interfaceNumber));
}

UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isochronousPacketCount) {
    if (isochronousPacketCount < 0) throw std::runtime_error(std::string(__func__) + ": isochronous packet count is negative");
    return reinterpret_cast<UsbdTransfer*>(libusb_alloc_transfer(isochronousPacketCount));
}

std::int32_t APS5_VABI sceUsbdSubmitTransfer(UsbdTransfer* transfer) {
    RequireTransfer(transfer);
    return ToHostError(libusb_submit_transfer(HostTransfer(transfer)));
}

std::int32_t APS5_VABI sceUsbdCancelTransfer(UsbdTransfer* transfer) {
    RequireTransfer(transfer);
    return ToHostError(libusb_cancel_transfer(HostTransfer(transfer)));
}

void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer) {
    RequireTransfer(transfer);
    Usbd::UnregisterTransferCallback(transfer);
    libusb_free_transfer(HostTransfer(transfer));
}

void APS5_VABI sceUsbdFillControlTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, unsigned char* buffer, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    RequireTransfer(transfer);
    RequireHandle(handle);
    if (buffer == nullptr) throw std::runtime_error(std::string(__func__) + ": control transfer buffer is null");
    auto* hostTransfer = HostTransfer(transfer);
    libusb_fill_control_transfer(hostTransfer, HostHandle(handle), buffer, nullptr, nullptr, timeout);
    Usbd::RegisterTransferCallback(transfer, callback, userData);
}

void APS5_VABI sceUsbdFillBulkTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    RequireTransfer(transfer);
    RequireHandle(handle);
    RequireBuffer(buffer, length);
    libusb_fill_bulk_transfer(HostTransfer(transfer), HostHandle(handle), endpoint, buffer, length, nullptr, nullptr, timeout);
    Usbd::RegisterTransferCallback(transfer, callback, userData);
}

void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    RequireTransfer(transfer);
    RequireHandle(handle);
    RequireBuffer(buffer, length);
    libusb_fill_interrupt_transfer(HostTransfer(transfer), HostHandle(handle), endpoint, buffer, length, nullptr, nullptr, timeout);
    Usbd::RegisterTransferCallback(transfer, callback, userData);
}

void APS5_VABI sceUsbdFillIsoTransfer(UsbdTransfer* transfer, UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* buffer, std::int32_t length, std::int32_t isochronousPacketCount, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    RequireTransfer(transfer);
    RequireHandle(handle);
    RequireBuffer(buffer, length);
    if (isochronousPacketCount < 0) throw std::runtime_error(std::string(__func__) + ": isochronous packet count is negative");
    libusb_fill_iso_transfer(HostTransfer(transfer), HostHandle(handle), endpoint, buffer, length, isochronousPacketCount, nullptr, nullptr, timeout);
    Usbd::RegisterTransferCallback(transfer, callback, userData);
}

void APS5_VABI sceUsbdSetIsoPacketLengths(UsbdTransfer* transfer, std::uint32_t length) {
    RequireTransfer(transfer);
    libusb_set_iso_packet_lengths(HostTransfer(transfer), length);
}

unsigned char* APS5_VABI sceUsbdGetIsoPacketBuffer(UsbdTransfer* transfer, std::uint32_t packet) {
    RequireTransfer(transfer);
    if (packet >= static_cast<std::uint32_t>(transfer->isochronousPacketCount)) throw std::runtime_error(std::string(__func__) + ": isochronous packet index is out of range");
    return libusb_get_iso_packet_buffer(HostTransfer(transfer), packet);
}

void APS5_VABI sceUsbdFillControlSetup(unsigned char* buffer, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length) {
    if (buffer == nullptr) throw std::runtime_error(std::string(__func__) + ": control setup buffer is null");
    libusb_fill_control_setup(buffer, requestType, request, value, index, length);
}

UsbdControlSetup* APS5_VABI sceUsbdControlTransferGetSetup(UsbdTransfer* transfer) {
    RequireTransfer(transfer);
    if (transfer->buffer == nullptr) throw std::runtime_error(std::string(__func__) + ": control transfer has no buffer");
    return reinterpret_cast<UsbdControlSetup*>(transfer->buffer);
}

unsigned char* APS5_VABI sceUsbdControlTransferGetData(UsbdTransfer* transfer) {
    RequireTransfer(transfer);
    if (transfer->buffer == nullptr) throw std::runtime_error(std::string(__func__) + ": control transfer has no buffer");
    return transfer->buffer + kControlSetupSize;
}

std::int32_t APS5_VABI sceUsbdControlTransfer(UsbdDeviceHandle* handle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, unsigned char* data, std::int32_t length, std::uint32_t timeout) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    if (length > 0xffff) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_control_transfer(HostHandle(handle), requestType, request, value, index, data, static_cast<std::uint16_t>(length), timeout));
}

std::int32_t APS5_VABI sceUsbdBulkTransfer(UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* data, std::int32_t length, std::int32_t* actualLength, std::uint32_t timeout) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    if (actualLength == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_bulk_transfer(HostHandle(handle), endpoint, data, length, actualLength, timeout));
}

std::int32_t APS5_VABI sceUsbdInterruptTransfer(UsbdDeviceHandle* handle, std::uint8_t endpoint, unsigned char* data, std::int32_t length, std::int32_t* actualLength, std::uint32_t timeout) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    if (actualLength == nullptr) return USBD_ERROR_INVALID_ARG;
    return ToHostError(libusb_interrupt_transfer(HostHandle(handle), endpoint, data, length, actualLength, timeout));
}

std::int32_t APS5_VABI sceUsbdGetDescriptor(UsbdDeviceHandle* handle, std::uint8_t descriptorType, std::uint8_t descriptorIndex, unsigned char* data, std::int32_t length) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    return ToHostError(libusb_get_descriptor(HostHandle(handle), descriptorType, descriptorIndex, data, length));
}

std::int32_t APS5_VABI sceUsbdGetStringDescriptor(UsbdDeviceHandle* handle, std::uint8_t descriptorIndex, std::uint16_t languageId, unsigned char* data, std::int32_t length) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    return ToHostError(libusb_get_string_descriptor(HostHandle(handle), descriptorIndex, languageId, data, length));
}

std::int32_t APS5_VABI sceUsbdGetStringDescriptorAscii(UsbdDeviceHandle* handle, std::uint8_t descriptorIndex, unsigned char* data, std::int32_t length) {
    RequireHandle(handle);
    RequireBuffer(data, length);
    return ToHostError(libusb_get_string_descriptor_ascii(HostHandle(handle), descriptorIndex, data, length));
}

std::int32_t APS5_VABI sceUsbdTryLockEvents() {
    return ToHostError(libusb_try_lock_events(HostContext()));
}

void APS5_VABI sceUsbdLockEvents() {
    libusb_lock_events(HostContext());
}

void APS5_VABI sceUsbdUnlockEvents() {
    libusb_unlock_events(HostContext());
}

std::int32_t APS5_VABI sceUsbdEventHandlingOk() {
    return ToHostError(libusb_event_handling_ok(HostContext()));
}

std::int32_t APS5_VABI sceUsbdEventHandlerActive() {
    return ToHostError(libusb_event_handler_active(HostContext()));
}

void APS5_VABI sceUsbdLockEventWaiters() {
    libusb_lock_event_waiters(HostContext());
}

void APS5_VABI sceUsbdUnlockEventWaiters() {
    libusb_unlock_event_waiters(HostContext());
}

std::int32_t APS5_VABI sceUsbdWaitForEvent(UsbdTimeval* timeout) {
    if (timeout == nullptr) return USBD_ERROR_INVALID_ARG;
    timeval hostTimeout{};
    hostTimeout.tv_sec = static_cast<decltype(hostTimeout.tv_sec)>(timeout->seconds);
    hostTimeout.tv_usec = static_cast<decltype(hostTimeout.tv_usec)>(timeout->microseconds);
    return ToHostError(libusb_wait_for_event(HostContext(), &hostTimeout));
}

std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(UsbdTimeval* timeout) {
    if (timeout == nullptr) return USBD_ERROR_INVALID_ARG;
    if (timeout->seconds < 0 || timeout->microseconds < 0 || timeout->microseconds >= 1000000) return USBD_ERROR_INVALID_ARG;
    timeval hostTimeout{};
    hostTimeout.tv_sec = static_cast<decltype(hostTimeout.tv_sec)>(timeout->seconds);
    hostTimeout.tv_usec = static_cast<decltype(hostTimeout.tv_usec)>(timeout->microseconds);
    return ToHostError(libusb_handle_events_timeout(HostContext(), &hostTimeout));
}

std::int32_t APS5_VABI sceUsbdHandleEvents() {
    return ToHostError(libusb_handle_events(HostContext()));
}

std::int32_t APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval* timeout) {
    if (timeout == nullptr) return USBD_ERROR_INVALID_ARG;
    if (timeout->seconds < 0 || timeout->microseconds < 0 || timeout->microseconds >= 1000000) return USBD_ERROR_INVALID_ARG;
    timeval hostTimeout{};
    hostTimeout.tv_sec = static_cast<decltype(hostTimeout.tv_sec)>(timeout->seconds);
    hostTimeout.tv_usec = static_cast<decltype(hostTimeout.tv_usec)>(timeout->microseconds);
    return ToHostError(libusb_handle_events_timeout_completed(HostContext(), &hostTimeout, nullptr));
}

std::int32_t APS5_VABI sceUsbdCheckConnected(UsbdDeviceHandle* handle) {
    RequireHandle(handle);
    int configuration = 0;
    return ToHostError(libusb_get_configuration(HostHandle(handle), &configuration));
}

}
