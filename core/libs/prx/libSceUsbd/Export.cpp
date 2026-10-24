#include "prx/libSceUsbd/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <libusb.h>

#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr std::int32_t SCE_USBD_ERROR_INVALID_ARG = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t SCE_USBD_ERROR_NOT_FOUND = static_cast<std::int32_t>(0x80240005);
constexpr std::int32_t SCE_USBD_ERROR_OTHER = static_cast<std::int32_t>(0x802400FF);
constexpr std::uint8_t TRANSFER_TYPE_CONTROL = 0;
constexpr std::uint8_t TRANSFER_TYPE_ISOCHRONOUS = 1;
constexpr std::uint8_t TRANSFER_TYPE_BULK = 2;
constexpr std::uint8_t TRANSFER_TYPE_INTERRUPT = 3;
constexpr std::int32_t CONTROL_SETUP_SIZE = 8;
constexpr std::uint8_t SUPPORTED_TRANSFER_FLAGS = LIBUSB_TRANSFER_SHORT_NOT_OK | LIBUSB_TRANSFER_ADD_ZERO_PACKET;
constexpr std::uint16_t STRING_DESCRIPTOR_SIZE = 255;
constexpr std::uint32_t STRING_DESCRIPTOR_TIMEOUT = 1000;

static_assert(offsetof(UsbdTransfer, timeout) == 12 && offsetof(UsbdTransfer, callback) == 32 && offsetof(UsbdTransfer, numIsoPackets) == 56 && offsetof(UsbdTransfer, isoPacketDescriptors) == 60);
static_assert(sizeof(libusb_device_descriptor) == 18);
static_assert(sizeof(libusb_config_descriptor) == 40 && offsetof(libusb_config_descriptor, interface) == 16 && offsetof(libusb_config_descriptor, extra) == 24 && offsetof(libusb_config_descriptor, extra_length) == 32);
static_assert(sizeof(libusb_interface) == 16 && offsetof(libusb_interface, num_altsetting) == 8);
static_assert(sizeof(libusb_interface_descriptor) == 40 && offsetof(libusb_interface_descriptor, endpoint) == 16 && offsetof(libusb_interface_descriptor, extra) == 24 && offsetof(libusb_interface_descriptor, extra_length) == 32);
static_assert(sizeof(libusb_endpoint_descriptor) == 32 && offsetof(libusb_endpoint_descriptor, wMaxPacketSize) == 4 && offsetof(libusb_endpoint_descriptor, extra) == 16 && offsetof(libusb_endpoint_descriptor, extra_length) == 24);
static_assert(sizeof(libusb_iso_packet_descriptor) == sizeof(UsbdIsoPacketDescriptor));

struct UsbdState {
    std::mutex mutex;
    libusb_context* context = nullptr;
    std::uint32_t initCount = 0;
    std::unordered_map<libusb_device*, std::uint32_t> devices;
    std::unordered_set<libusb_device**> lists;
    std::unordered_set<libusb_device_handle*> handles;
    std::unordered_set<libusb_config_descriptor*> configs;
    std::unordered_map<UsbdTransfer*, libusb_transfer*> inFlight;
};

UsbdState& State() {
    static auto* state = new UsbdState();
    return *state;
}

void* g_emptyDeviceList[1] = {nullptr};

std::string AbsentDevice(const char* function) {
    return std::string(function) + ": the device was not returned by sceUsbdGetDeviceList or was already released";
}

std::string UnopenedHandle(const char* function) {
    return std::string(function) + ": the handle was not returned by sceUsbdOpen or was already closed";
}

std::int32_t ToGuestError(int result) {
    if (result >= 0) return result;
    if (result == LIBUSB_ERROR_OTHER) return SCE_USBD_ERROR_OTHER;
    return static_cast<std::int32_t>(0x80240000u - static_cast<std::uint32_t>(result));
}

libusb_device* KnownDevice(void* device, const char* function) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device*>(device);
    if (!state.devices.contains(host)) throw std::runtime_error(AbsentDevice(function));
    return host;
}

libusb_device_handle* KnownHandle(void* deviceHandle, const char* function) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device_handle*>(deviceHandle);
    if (!state.handles.contains(host)) throw std::runtime_error(UnopenedHandle(function));
    return host;
}

void AddDeviceReference(UsbdState& state, libusb_device* device) {
    ++state.devices[device];
}

void DropDeviceReference(UsbdState& state, libusb_device* device) {
    const auto found = state.devices.find(device);
    if (found == state.devices.end()) throw std::runtime_error(AbsentDevice("sceUsbdUnrefDevice"));
    if (--found->second == 0) state.devices.erase(found);
}

void LIBUSB_CALL CompleteTransfer(libusb_transfer* host) {
    auto* guest = static_cast<UsbdTransfer*>(host->user_data);
    guest->status = host->status;
    guest->actualLength = host->actual_length;
    for (int packet = 0; packet < host->num_iso_packets; ++packet) {
        guest->isoPacketDescriptors[packet].actualLength = host->iso_packet_desc[packet].actual_length;
        guest->isoPacketDescriptors[packet].status = host->iso_packet_desc[packet].status;
    }
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        state.inFlight.erase(guest);
    }
    libusb_free_transfer(host);
    if (guest->callback != nullptr) guest->callback(guest);
}

bool RequestStringDescriptor(libusb_device_handle* handle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* descriptor) {
    std::memset(descriptor, 0, STRING_DESCRIPTOR_SIZE);
    const auto value = static_cast<std::uint16_t>(LIBUSB_DT_STRING << 8 | descriptorIndex);
    const int result = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, value, languageId, descriptor, STRING_DESCRIPTOR_SIZE, STRING_DESCRIPTOR_TIMEOUT);
    return result >= 0 && descriptor[1] == LIBUSB_DT_STRING;
}

}

extern "C" {

std::int32_t APS5_VABI sceUsbdInit() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initCount == 0) {
        libusb_context* context = nullptr;
        if (libusb_init_context(&context, nullptr, 0) == 0) state.context = context;
    }
    ++state.initCount;
    return 0;
}

void APS5_VABI sceUsbdExit() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initCount == 0 || --state.initCount != 0) return;
    if (!state.inFlight.empty()) throw std::runtime_error(std::string(__func__) + ": transfers are still in flight");
    for (auto* handle : state.handles) libusb_close(handle);
    for (auto* config : state.configs) libusb_free_config_descriptor(config);
    for (auto* list : state.lists) libusb_free_device_list(list, 0);
    for (const auto& [device, references] : state.devices) {
        for (std::uint32_t reference = 0; reference < references; ++reference) libusb_unref_device(device);
    }
    state.handles.clear();
    state.configs.clear();
    state.lists.clear();
    state.devices.clear();
    if (state.context != nullptr) libusb_exit(state.context);
    state.context = nullptr;
}

std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list) {
    if (list == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.context == nullptr) {
        *list = g_emptyDeviceList;
        return 0;
    }
    libusb_device** devices = nullptr;
    const ssize_t count = libusb_get_device_list(state.context, &devices);
    if (count < 0) return ToGuestError(static_cast<int>(count));
    for (ssize_t index = 0; index < count; ++index) AddDeviceReference(state, devices[index]);
    state.lists.insert(devices);
    *list = reinterpret_cast<void**>(devices);
    return count;
}

void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices) {
    if (list == nullptr || list == g_emptyDeviceList) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* devices = reinterpret_cast<libusb_device**>(list);
    if (state.lists.erase(devices) == 0) throw std::runtime_error(std::string(__func__) + ": list was not returned by sceUsbdGetDeviceList");
    if (unrefDevices != 0) {
        for (auto** device = devices; *device != nullptr; ++device) DropDeviceReference(state, *device);
    }
    libusb_free_device_list(devices, unrefDevices);
}

std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout) {
    if (timeout == nullptr || timeout->seconds < 0 || timeout->microseconds < 0 || timeout->microseconds >= 1000000) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_context* context;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        context = state.context;
    }
    if (context == nullptr) {
        std::this_thread::sleep_for(std::chrono::seconds(timeout->seconds) + std::chrono::microseconds(timeout->microseconds));
        return 0;
    }
    timeval host{};
    host.tv_sec = static_cast<decltype(host.tv_sec)>(timeout->seconds);
    host.tv_usec = static_cast<decltype(host.tv_usec)>(timeout->microseconds);
    return ToGuestError(libusb_handle_events_timeout_completed(context, &host, nullptr));
}

UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets) {
    if (isoPackets < 0) return nullptr;
    const std::size_t size = offsetof(UsbdTransfer, isoPacketDescriptors) + sizeof(UsbdIsoPacketDescriptor) * static_cast<std::size_t>(isoPackets);
    auto* transfer = static_cast<UsbdTransfer*>(std::calloc(1, size));
    if (transfer == nullptr) return nullptr;
    transfer->numIsoPackets = isoPackets;
    return transfer;
}

void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer) {
    if (transfer == nullptr) return;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (state.inFlight.contains(transfer)) throw std::runtime_error(std::string(__func__) + ": transfer is still in flight");
    }
    std::free(transfer);
}

void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    transfer->deviceHandle = deviceHandle;
    transfer->endpoint = endpoint;
    transfer->type = TRANSFER_TYPE_INTERRUPT;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->userData = userData;
    transfer->callback = callback;
}

void APS5_VABI sceUsbdFillBulkTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    transfer->deviceHandle = deviceHandle;
    transfer->endpoint = endpoint;
    transfer->type = TRANSFER_TYPE_BULK;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->userData = userData;
    transfer->callback = callback;
}

void APS5_VABI sceUsbdFillIsoTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, std::int32_t numIsoPackets, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    transfer->deviceHandle = deviceHandle;
    transfer->endpoint = endpoint;
    transfer->type = TRANSFER_TYPE_ISOCHRONOUS;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->numIsoPackets = numIsoPackets;
    transfer->userData = userData;
    transfer->callback = callback;
}

void APS5_VABI sceUsbdFillControlSetup(std::uint8_t* buffer, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length) {
    if (buffer == nullptr) throw std::runtime_error(std::string(__func__) + ": null buffer");
    buffer[0] = requestType;
    buffer[1] = request;
    buffer[2] = static_cast<std::uint8_t>(value);
    buffer[3] = static_cast<std::uint8_t>(value >> 8);
    buffer[4] = static_cast<std::uint8_t>(index);
    buffer[5] = static_cast<std::uint8_t>(index >> 8);
    buffer[6] = static_cast<std::uint8_t>(length);
    buffer[7] = static_cast<std::uint8_t>(length >> 8);
}

void APS5_VABI sceUsbdFillControlTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t* buffer, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    transfer->deviceHandle = deviceHandle;
    transfer->endpoint = 0;
    transfer->type = TRANSFER_TYPE_CONTROL;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = buffer != nullptr ? CONTROL_SETUP_SIZE + (buffer[6] | buffer[7] << 8) : 0;
    transfer->userData = userData;
    transfer->callback = callback;
}

std::uint8_t* APS5_VABI sceUsbdControlTransferGetData(UsbdTransfer* transfer) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    return transfer->buffer != nullptr ? transfer->buffer + CONTROL_SETUP_SIZE : nullptr;
}

std::uint8_t* APS5_VABI sceUsbdControlTransferGetSetup(UsbdTransfer* transfer) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    return transfer->buffer;
}

void APS5_VABI sceUsbdSetIsoPacketLengths(UsbdTransfer* transfer, std::uint32_t length) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    for (std::int32_t packet = 0; packet < transfer->numIsoPackets; ++packet) transfer->isoPacketDescriptors[packet].length = length;
}

std::uint8_t* APS5_VABI sceUsbdGetIsoPacketBuffer(UsbdTransfer* transfer, std::uint32_t packet) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    if (transfer->numIsoPackets < 0 || packet >= static_cast<std::uint32_t>(transfer->numIsoPackets) || transfer->buffer == nullptr) return nullptr;
    std::uint8_t* pointer = transfer->buffer;
    for (std::uint32_t index = 0; index < packet; ++index) pointer += transfer->isoPacketDescriptors[index].length;
    return pointer;
}

std::int32_t APS5_VABI sceUsbdBulkTransfer(void* deviceHandle, std::uint8_t endpoint, std::uint8_t* data, std::int32_t length, std::int32_t* transferred, std::uint32_t timeout) {
    if (deviceHandle == nullptr || (length != 0 && data == nullptr)) return SCE_USBD_ERROR_INVALID_ARG;
    if (transferred == nullptr) throw std::runtime_error(std::string(__func__) + ": null transferred output");
    *transferred = 0;
    return ToGuestError(libusb_bulk_transfer(KnownHandle(deviceHandle, __func__), endpoint, data, length, transferred, timeout));
}

std::int32_t APS5_VABI sceUsbdInterruptTransfer(void* deviceHandle, std::uint8_t endpoint, std::uint8_t* data, std::int32_t length, std::int32_t* transferred, std::uint32_t timeout) {
    if (deviceHandle == nullptr || (length != 0 && data == nullptr)) return SCE_USBD_ERROR_INVALID_ARG;
    if (transferred == nullptr) throw std::runtime_error(std::string(__func__) + ": null transferred output");
    *transferred = 0;
    return ToGuestError(libusb_interrupt_transfer(KnownHandle(deviceHandle, __func__), endpoint, data, length, transferred, timeout));
}

std::int32_t APS5_VABI sceUsbdCancelTransfer(UsbdTransfer* transfer) {
    if (transfer == nullptr) throw std::runtime_error(std::string(__func__) + ": null transfer");
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto found = state.inFlight.find(transfer);
    if (found == state.inFlight.end()) return SCE_USBD_ERROR_NOT_FOUND;
    return ToGuestError(libusb_cancel_transfer(found->second));
}

std::int32_t APS5_VABI sceUsbdEventHandlingOk() {
    return 1;
}

void APS5_VABI sceUsbdFreeConfigDescriptor(void* config) {
    if (config == nullptr) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_config_descriptor*>(config);
    if (state.configs.erase(host) == 0) throw std::runtime_error(std::string(__func__) + ": descriptor was not returned by this library");
    libusb_free_config_descriptor(host);
}

std::int32_t APS5_VABI sceUsbdAttachKernelDriver(void* deviceHandle, std::int32_t interfaceNumber) {
    (void)interfaceNumber;
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    KnownHandle(deviceHandle, __func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdCheckConnected(void* deviceHandle) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    int configuration = 0;
    const int result = libusb_get_configuration(KnownHandle(deviceHandle, __func__), &configuration);
    return result == LIBUSB_ERROR_NO_DEVICE ? ToGuestError(LIBUSB_ERROR_NO_DEVICE) : 0;
}

std::int32_t APS5_VABI sceUsbdClaimInterface(void* deviceHandle, std::int32_t interfaceNumber) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_claim_interface(KnownHandle(deviceHandle, __func__), interfaceNumber));
}

void APS5_VABI sceUsbdClose(void* deviceHandle) {
    if (deviceHandle == nullptr) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device_handle*>(deviceHandle);
    if (state.handles.erase(host) == 0) throw std::runtime_error(UnopenedHandle(__func__));
    libusb_close(host);
}

std::int32_t APS5_VABI sceUsbdControlTransfer(void* deviceHandle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint8_t* data, std::uint16_t length, std::uint32_t timeout) {
    if (deviceHandle == nullptr || (length != 0 && data == nullptr)) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_control_transfer(KnownHandle(deviceHandle, __func__), requestType, request, value, index, data, length, timeout));
}

std::int32_t APS5_VABI sceUsbdGetActiveConfigDescriptor(void* device, void** config) {
    if (device == nullptr) throw std::runtime_error(std::string(__func__) + ": null device");
    if (config == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_config_descriptor* host = nullptr;
    const int result = libusb_get_active_config_descriptor(KnownDevice(device, __func__), &host);
    *config = nullptr;
    if (result != 0) return ToGuestError(result);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.configs.insert(host);
    *config = host;
    return 0;
}

std::uint8_t APS5_VABI sceUsbdGetBusNumber(void* device) {
    if (device == nullptr) return 0;
    return libusb_get_bus_number(KnownDevice(device, __func__));
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config) {
    if (device == nullptr || config == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_config_descriptor* host = nullptr;
    const int result = libusb_get_config_descriptor(KnownDevice(device, __func__), configIndex, &host);
    *config = nullptr;
    if (result != 0) return ToGuestError(result);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.configs.insert(host);
    *config = host;
    return 0;
}

std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(void* device) {
    if (device == nullptr) return 0;
    return libusb_get_device_address(KnownDevice(device, __func__));
}

std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor) {
    if (device == nullptr || descriptor == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_get_device_descriptor(KnownDevice(device, __func__), static_cast<libusb_device_descriptor*>(descriptor)));
}

std::int32_t APS5_VABI sceUsbdGetStringDescriptor(void* deviceHandle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* data, std::int32_t length) {
    if (deviceHandle == nullptr || data == nullptr || length < 1) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(deviceHandle, __func__);
    if (length > 65535) length = 65535;
    data[0] = 0;
    return ToGuestError(libusb_get_string_descriptor(handle, descriptorIndex, languageId, data, length));
}

std::int32_t APS5_VABI sceUsbdKernelDriverActive(void* deviceHandle, std::int32_t interfaceNumber) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_kernel_driver_active(KnownHandle(deviceHandle, __func__), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdOpen(void* device, void** deviceHandle) {
    if (device == nullptr) throw std::runtime_error(std::string(__func__) + ": null device");
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    *deviceHandle = nullptr;
    libusb_device_handle* host = nullptr;
    const int result = libusb_open(KnownDevice(device, __func__), &host);
    if (result != 0) return ToGuestError(result);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.handles.insert(host);
    *deviceHandle = host;
    return 0;
}

void* APS5_VABI sceUsbdRefDevice(void* device) {
    if (device == nullptr) return nullptr;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device*>(device);
    if (!state.devices.contains(host)) throw std::runtime_error(AbsentDevice(__func__));
    AddDeviceReference(state, host);
    return libusb_ref_device(host);
}

std::int32_t APS5_VABI sceUsbdReleaseInterface(void* deviceHandle, std::int32_t interfaceNumber) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_release_interface(KnownHandle(deviceHandle, __func__), interfaceNumber));
}

std::int32_t APS5_VABI sceUsbdResetDevice(void* deviceHandle) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_reset_device(KnownHandle(deviceHandle, __func__)));
}

std::int32_t APS5_VABI sceUsbdSetConfiguration(void* deviceHandle, std::int32_t configuration) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return ToGuestError(libusb_set_configuration(KnownHandle(deviceHandle, __func__), configuration));
}

std::int32_t APS5_VABI sceUsbdSubmitTransfer(UsbdTransfer* transfer) {
    if (transfer == nullptr || transfer->deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(transfer->deviceHandle, __func__);
    if ((transfer->flags & ~SUPPORTED_TRANSFER_FLAGS) != 0) throw std::runtime_error(std::string(__func__) + ": transfer flags " + std::to_string(transfer->flags) + " are not supported");
    if (transfer->numIsoPackets < 0) return SCE_USBD_ERROR_INVALID_ARG;
    auto* host = libusb_alloc_transfer(transfer->numIsoPackets);
    if (host == nullptr) return ToGuestError(LIBUSB_ERROR_NO_MEM);
    host->dev_handle = handle;
    host->flags = transfer->flags;
    host->endpoint = transfer->endpoint;
    host->type = transfer->type;
    host->timeout = transfer->timeout;
    host->length = transfer->length;
    host->buffer = transfer->buffer;
    host->user_data = transfer;
    host->callback = CompleteTransfer;
    host->num_iso_packets = transfer->numIsoPackets;
    for (int packet = 0; packet < transfer->numIsoPackets; ++packet) host->iso_packet_desc[packet].length = transfer->isoPacketDescriptors[packet].length;
    auto& state = State();
    {
        std::lock_guard lock(state.mutex);
        if (!state.inFlight.emplace(transfer, host).second) {
            libusb_free_transfer(host);
            return ToGuestError(LIBUSB_ERROR_BUSY);
        }
    }
    const int result = libusb_submit_transfer(host);
    if (result != 0) {
        {
            std::lock_guard lock(state.mutex);
            state.inFlight.erase(transfer);
        }
        libusb_free_transfer(host);
    }
    return ToGuestError(result);
}

void APS5_VABI sceUsbdUnrefDevice(void* device) {
    if (device == nullptr) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device*>(device);
    DropDeviceReference(state, host);
    libusb_unref_device(host);
}

std::int32_t APS5_VABI sceUsbdClearHalt(void* deviceHandle, std::uint8_t endpoint) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdDetachKernelDriver(void* deviceHandle, int interfaceNumber) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdEventHandlerActive() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptorByValue(void* device, std::uint8_t configurationValue, void** config) {
    if (device == nullptr || config == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_config_descriptor* host = nullptr;
    const int result = libusb_get_config_descriptor_by_value(KnownDevice(device, __func__), configurationValue, &host);
    *config = nullptr;
    if (result != 0) return ToGuestError(result);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.configs.insert(host);
    *config = host;
    return 0;
}

std::int32_t APS5_VABI sceUsbdGetConfiguration(void* deviceHandle, std::int32_t* configuration) {
    if (deviceHandle == nullptr || configuration == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    int host = 0;
    const int result = libusb_get_configuration(KnownHandle(deviceHandle, __func__), &host);
    if (result == 0) *configuration = host;
    return ToGuestError(result);
}

std::int32_t APS5_VABI sceUsbdGetDescriptor(void* deviceHandle, std::uint8_t descriptorType, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length) {
    if (deviceHandle == nullptr || data == nullptr || length < 1) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(deviceHandle, __func__);
    if (length > 65535) length = 65535;
    return ToGuestError(libusb_get_descriptor(handle, descriptorType, descriptorIndex, data, length));
}

void* APS5_VABI sceUsbdGetDevice(void* deviceHandle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetDeviceSpeed(void* device) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdGetMaxIsoPacketSize(void* device, std::uint8_t endpoint) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(void* device, std::uint8_t endpoint) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdGetStringDescriptorAscii(void* deviceHandle, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length) {
    if (deviceHandle == nullptr || data == nullptr || length < 1) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(deviceHandle, __func__);
    if (length > 65535) length = 65535;
    data[0] = 0;
    std::uint8_t descriptor[STRING_DESCRIPTOR_SIZE];
    if (!RequestStringDescriptor(handle, 0, 0, descriptor)) return SCE_USBD_ERROR_OTHER;
    const auto languageId = static_cast<std::uint16_t>(descriptor[2] | descriptor[3] << 8);
    if (!RequestStringDescriptor(handle, descriptorIndex, languageId, descriptor) || descriptor[0] < 2) return SCE_USBD_ERROR_OTHER;
    std::int32_t units = descriptor[0] / 2 - 1;
    if (units > length - 1) units = length - 1;
    std::uint8_t byteOrder = 3;
    std::int32_t written = 0;
    for (std::int32_t unit = 0; unit < units; ++unit) {
        const auto code = static_cast<std::uint16_t>(descriptor[2 * unit + 2] | descriptor[2 * unit + 3] << 8);
        std::uint8_t character;
        if ((code & 0xFF00) == 0 && (byteOrder & 1) != 0) {
            character = static_cast<std::uint8_t>(code);
            byteOrder = 1;
        } else if ((code & 0x00FF) == 0 && (byteOrder & 2) != 0) {
            character = static_cast<std::uint8_t>(code >> 8);
            byteOrder = 2;
        } else {
            continue;
        }
        if (character < 0x20 || character > 0x7E || character == '<' || character == '>') continue;
        data[written++] = character;
    }
    data[written] = 0;
    return written;
}

std::int32_t APS5_VABI sceUsbdHandleEvents() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval* timeout) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceUsbdLockEventWaiters() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceUsbdLockEvents() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdSetInterfaceAltSetting(void* deviceHandle, int interfaceNumber, int alternateSetting) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdTryLockEvents() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceUsbdUnlockEventWaiters() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceUsbdUnlockEvents() {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI sceUsbdWaitForEvent(UsbdTimeval* timeout) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
