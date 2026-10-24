#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <libusb.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
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
    std::size_t activeDeviceCalls = 0;
    std::unordered_map<libusb_device*, std::uint32_t> devices;
    std::unordered_set<libusb_device**> lists;
    std::unordered_set<libusb_device_handle*> handles;
    std::unordered_map<libusb_device_handle*, std::uint32_t> claimed;
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

bool IsKnownDevice(const UsbdState& state, libusb_device* device) {
    return state.devices.contains(device) || std::any_of(state.handles.begin(), state.handles.end(), [device](libusb_device_handle* handle) { return libusb_get_device(handle) == device; });
}

class DeviceReference {
public:
    DeviceReference(void* device, const char* function) {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        auto* host = static_cast<libusb_device*>(device);
        if (!IsKnownDevice(state, host)) throw std::runtime_error(AbsentDevice(function));
        this->device = libusb_ref_device(host);
        ++state.activeDeviceCalls;
    }

    DeviceReference(const DeviceReference&) = delete;
    DeviceReference& operator=(const DeviceReference&) = delete;

    ~DeviceReference() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        libusb_unref_device(this->device);
        --state.activeDeviceCalls;
    }

    libusb_device* Get() const {
        return this->device;
    }

private:
    libusb_device* device;
};

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

struct UsbdEvents {
    std::mutex lock;
    std::condition_variable changed;
    std::size_t waiters = 0;
    std::atomic<std::thread::id> owner{};
    std::atomic<std::thread::id> handler{};
    std::atomic<std::uint64_t> completions{0};
};

UsbdEvents& Events() {
    static auto* events = new UsbdEvents();
    return *events;
}

bool ValidTimeout(const UsbdTimeval* timeout) {
    return timeout == nullptr || (timeout->seconds >= 0 && timeout->seconds <= INT_MAX && timeout->microseconds >= 0 && timeout->microseconds < 1000000);
}

void AcquireEvents(UsbdEvents& events, const char* function) {
    const auto self = std::this_thread::get_id();
    if (events.owner.load() == self) throw std::runtime_error(std::string(function) + ": this thread already holds the event lock");
    events.lock.lock();
    {
        std::lock_guard stateLock(State().mutex);
        if (State().initCount == 0) {
            events.lock.unlock();
            throw std::runtime_error(std::string(function) + ": USB is not initialized");
        }
    }
    events.owner.store(self);
}

void RequireEventLock(UsbdEvents& events, const char* function) {
    if (events.owner.load() != std::this_thread::get_id()) throw std::runtime_error(std::string(function) + ": this thread does not hold the event lock");
}

void ReleaseEvents(UsbdEvents& events, const char* function) {
    RequireEventLock(events, function);
    events.owner.store(std::thread::id{});
    events.lock.unlock();
}

void LockEventsAndClaim(UsbdEvents& events, const char* function) {
    AcquireEvents(events, function);
    if (events.handler.load() == std::thread::id{}) events.handler.store(std::this_thread::get_id());
}

void UnlockEventsAndRelease(UsbdEvents& events, const char* function) {
    RequireEventLock(events, function);
    if (events.handler.load() == std::this_thread::get_id()) {
        events.handler.store(std::thread::id{});
        events.changed.notify_all();
    }
    ReleaseEvents(events, function);
}

int WaitForChange(UsbdEvents& events, const UsbdTimeval* timeout) {
    const auto self = std::this_thread::get_id();
    std::unique_lock guard(events.lock, std::adopt_lock);
    events.owner.store(std::thread::id{});
    bool timedOut = false;
    ++events.waiters;
    try {
        if (timeout == nullptr) {
            events.changed.wait(guard);
        } else {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout->seconds) + std::chrono::microseconds(timeout->microseconds);
            timedOut = events.changed.wait_until(guard, deadline) == std::cv_status::timeout;
        }
    } catch (...) {
        --events.waiters;
        events.owner.store(self);
        guard.release();
        throw;
    }
    --events.waiters;
    guard.release();
    events.owner.store(self);
    if (events.handler.load() == std::thread::id{}) events.handler.store(self);
    return timedOut ? 1 : 0;
}

libusb_context* CurrentContext();

int HandleAsHandler(UsbdEvents& events, const UsbdTimeval* timeout) {
    const auto self = std::this_thread::get_id();
    const auto before = events.completions.load();
    events.owner.store(std::thread::id{});
    events.lock.unlock();
    int result = 0;
    try {
        libusb_context* context = CurrentContext();
        if (timeout != nullptr && context == nullptr) {
            std::this_thread::sleep_for(std::chrono::seconds(timeout->seconds) + std::chrono::microseconds(timeout->microseconds));
        } else if (timeout != nullptr) {
            timeval host{};
            host.tv_sec = static_cast<decltype(host.tv_sec)>(timeout->seconds);
            host.tv_usec = static_cast<decltype(host.tv_usec)>(timeout->microseconds);
            result = libusb_handle_events_timeout_completed(context, &host, nullptr);
        } else {
            while (result == 0 && events.completions.load() == before) {
                if (context == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    timeval slice{};
                    slice.tv_sec = 1;
                    result = libusb_handle_events_timeout_completed(context, &slice, nullptr);
                }
            }
        }
    } catch (...) {
        events.lock.lock();
        events.owner.store(self);
        throw;
    }
    events.lock.lock();
    events.owner.store(self);
    return result;
}

std::int32_t HandleEventsLocked(UsbdEvents& events, const UsbdTimeval* timeout) {
    if (events.handler.load() == std::this_thread::get_id()) return ToGuestError(HandleAsHandler(events, timeout));
    return WaitForChange(events, timeout) != 0 ? ToGuestError(LIBUSB_ERROR_TIMEOUT) : 0;
}

std::int32_t RunEvents(const UsbdTimeval* timeout, const char* function) {
    auto& events = Events();
    LockEventsAndClaim(events, function);
    std::int32_t result;
    try {
        result = HandleEventsLocked(events, timeout);
    } catch (...) {
        UnlockEventsAndRelease(events, function);
        throw;
    }
    UnlockEventsAndRelease(events, function);
    return result;
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
    Events().completions.fetch_add(1);
    if (guest->callback != nullptr) guest->callback(guest);
}

libusb_context* CurrentContext() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.context;
}

int MaxPacketSize(libusb_device* device, std::uint8_t endpoint) {
    libusb_config_descriptor* config = nullptr;
    const int result = libusb_get_active_config_descriptor(device, &config);
    if (result < 0) return result;
    int size = LIBUSB_ERROR_NOT_FOUND;
    for (int index = 0; index < config->bNumInterfaces && size == LIBUSB_ERROR_NOT_FOUND; ++index) {
        const auto& entry = config->interface[index];
        for (int alternate = 0; alternate < entry.num_altsetting && size == LIBUSB_ERROR_NOT_FOUND; ++alternate) {
            const auto& setting = entry.altsetting[alternate];
            for (int item = 0; item < setting.bNumEndpoints; ++item) {
                if (setting.endpoint[item].bEndpointAddress == endpoint) {
                    size = setting.endpoint[item].wMaxPacketSize;
                    break;
                }
            }
        }
    }
    libusb_free_config_descriptor(config);
    return size;
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
    if (state.initCount == 0) return;
    if (state.initCount > 1) { --state.initCount; return; }
    if (state.activeDeviceCalls != 0) throw std::runtime_error(std::string(__func__) + ": device operations are active");
    if (!state.inFlight.empty()) throw std::runtime_error(std::string(__func__) + ": transfers are still in flight");
    auto& events = Events();
    if (events.owner.load() == std::this_thread::get_id() || events.handler.load() != std::thread::id{})
        throw std::runtime_error(std::string(__func__) + ": event handling is active");
    std::unique_lock eventLock(events.lock, std::try_to_lock);
    if (!eventLock.owns_lock() || events.waiters != 0)
        throw std::runtime_error(std::string(__func__) + ": event waiters are active");
    state.initCount = 0;
    for (auto* handle : state.handles) libusb_close(handle);
    state.claimed.clear();
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
    if (!ValidTimeout(timeout)) throw std::runtime_error(std::string(__func__) + ": a timeout of more than INT_MAX seconds is not supported");
    return RunEvents(timeout, __func__);
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
    return Events().handler.load() == std::this_thread::get_id() ? 1 : 0;
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
    auto* handle = KnownHandle(deviceHandle, __func__);
    const int result = libusb_claim_interface(handle, interfaceNumber);
    if (result == 0) {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        state.claimed[handle] |= 1u << interfaceNumber;
    }
    return ToGuestError(result);
}

void APS5_VABI sceUsbdClose(void* deviceHandle) {
    if (deviceHandle == nullptr) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* host = static_cast<libusb_device_handle*>(deviceHandle);
    if (!state.handles.contains(host)) throw std::runtime_error(UnopenedHandle(__func__));
    for (const auto& [transfer, pending] : state.inFlight) {
        if (transfer->deviceHandle == deviceHandle) throw std::runtime_error(std::string(__func__) + ": transfers must complete before closing the handle");
    }
    state.handles.erase(host);
    state.claimed.erase(host);
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
    const DeviceReference reference(device, __func__);
    const int result = libusb_get_active_config_descriptor(reference.Get(), &host);
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
    const DeviceReference reference(device, __func__);
    return libusb_get_bus_number(reference.Get());
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config) {
    if (device == nullptr || config == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_config_descriptor* host = nullptr;
    const DeviceReference reference(device, __func__);
    const int result = libusb_get_config_descriptor(reference.Get(), configIndex, &host);
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
    const DeviceReference reference(device, __func__);
    return libusb_get_device_address(reference.Get());
}

std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor) {
    if (device == nullptr || descriptor == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    const DeviceReference reference(device, __func__);
    return ToGuestError(libusb_get_device_descriptor(reference.Get(), static_cast<libusb_device_descriptor*>(descriptor)));
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
    const DeviceReference reference(device, __func__);
    const int result = libusb_open(reference.Get(), &host);
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
    if (!IsKnownDevice(state, host)) throw std::runtime_error(AbsentDevice(__func__));
    AddDeviceReference(state, host);
    return libusb_ref_device(host);
}

std::int32_t APS5_VABI sceUsbdReleaseInterface(void* deviceHandle, std::int32_t interfaceNumber) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(deviceHandle, __func__);
    const int result = libusb_release_interface(handle, interfaceNumber);
    if (result == 0) {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        state.claimed[handle] &= ~(1u << interfaceNumber);
    }
    return ToGuestError(result);
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
    if (deviceHandle == nullptr) throw std::runtime_error(std::string(__func__) + ": null handle");
    const int result = libusb_clear_halt(KnownHandle(deviceHandle, __func__), endpoint);
    if (result == 0) return 0;
    if (result == LIBUSB_ERROR_NOT_FOUND) return SCE_USBD_ERROR_OTHER;
    throw std::runtime_error(std::string(__func__) + ": libusb error " + std::to_string(result) + " has no known console result");
}

std::int32_t APS5_VABI sceUsbdDetachKernelDriver(void* deviceHandle, int interfaceNumber) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    return libusb_detach_kernel_driver(KnownHandle(deviceHandle, __func__), static_cast<std::uint8_t>(interfaceNumber)) == 0 ? 0 : SCE_USBD_ERROR_OTHER;
}

std::int32_t APS5_VABI sceUsbdEventHandlerActive() {
    return Events().handler.load() != std::thread::id{} ? 1 : 0;
}

std::int32_t APS5_VABI sceUsbdGetConfigDescriptorByValue(void* device, std::uint8_t configurationValue, void** config) {
    if (device == nullptr || config == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    libusb_config_descriptor* host = nullptr;
    const DeviceReference reference(device, __func__);
    const int result = libusb_get_config_descriptor_by_value(reference.Get(), configurationValue, &host);
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
    if (deviceHandle == nullptr) return nullptr;
    return libusb_get_device(KnownHandle(deviceHandle, __func__));
}

int APS5_VABI sceUsbdGetDeviceSpeed(void* device) {
    if (device == nullptr) return LIBUSB_SPEED_UNKNOWN;
    const DeviceReference reference(device, __func__);
    return std::min(libusb_get_device_speed(reference.Get()), static_cast<int>(LIBUSB_SPEED_SUPER));
}

std::int32_t APS5_VABI sceUsbdGetMaxIsoPacketSize(void* device, std::uint8_t endpoint) {
    if (device == nullptr) throw std::runtime_error(std::string(__func__) + ": null device");
    const DeviceReference reference(device, __func__);
    auto* host = reference.Get();
    int size = MaxPacketSize(host, endpoint);
    const int speed = libusb_get_device_speed(host);
    if (speed != LIBUSB_SPEED_LOW && speed != LIBUSB_SPEED_FULL && size >= 0) size = (size & 0x7FF) * std::min(1 + ((size >> 11) & 3), 3);
    return ToGuestError(size);
}

std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(void* device, std::uint8_t endpoint) {
    if (device == nullptr) return ToGuestError(LIBUSB_ERROR_NO_DEVICE);
    const DeviceReference reference(device, __func__);
    return ToGuestError(MaxPacketSize(reference.Get(), endpoint));
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
    return RunEvents(nullptr, __func__);
}

std::int32_t APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval* timeout) {
    if (!ValidTimeout(timeout)) throw std::runtime_error(std::string(__func__) + ": a negative or oversized timeout has no known result");
    auto& events = Events();
    RequireEventLock(events, __func__);
    return HandleEventsLocked(events, timeout);
}

void APS5_VABI sceUsbdLockEventWaiters() {
    AcquireEvents(Events(), __func__);
}

void APS5_VABI sceUsbdLockEvents() {
    LockEventsAndClaim(Events(), __func__);
}

void* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.context == nullptr) return nullptr;
    libusb_device** devices = nullptr;
    const ssize_t count = libusb_get_device_list(state.context, &devices);
    if (count < 0) return nullptr;
    libusb_device_handle* handle = nullptr;
    for (ssize_t index = 0; index < count; ++index) {
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(devices[index], &descriptor) == 0 && descriptor.idVendor == vendorId && descriptor.idProduct == productId) {
            if (libusb_open(devices[index], &handle) != 0) handle = nullptr;
            break;
        }
    }
    libusb_free_device_list(devices, 1);
    if (handle != nullptr) state.handles.insert(handle);
    return handle;
}

std::int32_t APS5_VABI sceUsbdSetInterfaceAltSetting(void* deviceHandle, int interfaceNumber, int alternateSetting) {
    if (deviceHandle == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    auto* handle = KnownHandle(deviceHandle, __func__);
    if (interfaceNumber < 0 || interfaceNumber > 31) return SCE_USBD_ERROR_INVALID_ARG;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        const auto claimed = state.claimed.find(handle);
        if (claimed == state.claimed.end() || (claimed->second & (1u << interfaceNumber)) == 0) return ToGuestError(LIBUSB_ERROR_NOT_FOUND);
        for (const auto& [guest, host] : state.inFlight) {
            if (host->dev_handle == handle) throw std::runtime_error(std::string(__func__) + ": the device has transfers in flight, which FreeBSD libusb cancels first");
        }
    }
    return libusb_set_interface_alt_setting(handle, interfaceNumber, static_cast<std::uint8_t>(alternateSetting)) == 0 ? 0 : SCE_USBD_ERROR_OTHER;
}

std::int32_t APS5_VABI sceUsbdTryLockEvents() {
    auto& events = Events();
    const auto self = std::this_thread::get_id();
    if (events.owner.load() == self || !events.lock.try_lock()) return 1;
    {
        std::lock_guard stateLock(State().mutex);
        if (State().initCount == 0) { events.lock.unlock(); return 1; }
    }
    if (events.handler.load() != std::thread::id{}) {
        events.lock.unlock();
        return 1;
    }
    events.owner.store(self);
    events.handler.store(self);
    return 0;
}

void APS5_VABI sceUsbdUnlockEventWaiters() {
    ReleaseEvents(Events(), __func__);
}

void APS5_VABI sceUsbdUnlockEvents() {
    UnlockEventsAndRelease(Events(), __func__);
}

std::int32_t APS5_VABI sceUsbdWaitForEvent(UsbdTimeval* timeout) {
    if (!ValidTimeout(timeout)) throw std::runtime_error(std::string(__func__) + ": a negative or oversized timeout has no known result");
    auto& events = Events();
    RequireEventLock(events, __func__);
    return WaitForChange(events, timeout);
}

}
