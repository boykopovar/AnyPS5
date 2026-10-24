#include <libusb.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <thread>

namespace {

int hostSpeed = LIBUSB_SPEED_HIGH;
int descriptorError = 0;
int descriptorReads = 0;
int descriptorFrees = 0;
libusb_config_descriptor configuration{};
libusb_device* fixtureDevice = nullptr;
int hostReferences = 0;
int openCalls = 0;
int closeCalls = 0;
int contextExits = 0;
bool openFails = false;
int operationResult = 0;
int operationCalls = 0;
int lastInterface = 0;
int lastAlternate = 0;
std::uint8_t lastEndpoint = 0;
int eventResult = 0;
int eventCalls = 0;
bool eventThrows = false;
bool deviceThrows = false;
bool unboundedEvent = false;
timeval eventTimeout{};
void (*eventProbe)() = nullptr;
void (*deviceProbe)() = nullptr;
std::array<int, 2> handleTokens{};

int PumpEvents(libusb_context*) {
    ++eventCalls;
    unboundedEvent = true;
    if (eventProbe) eventProbe();
    if (eventThrows) throw std::runtime_error("backend event failure");
    return eventResult;
}

int PumpEventsTimeout(libusb_context* context, timeval* timeout, int*) {
    eventTimeout = *timeout;
    const int result = PumpEvents(context);
    unboundedEvent = false;
    return result;
}

int ClearHalt(libusb_device_handle*, std::uint8_t endpoint) {
    ++operationCalls;
    lastEndpoint = endpoint;
    return operationResult;
}

int DetachDriver(libusb_device_handle*, int interfaceNumber) {
    ++operationCalls;
    lastInterface = interfaceNumber;
    return operationResult;
}

int SetAlternate(libusb_device_handle*, int interfaceNumber, int alternate) {
    ++operationCalls;
    lastInterface = interfaceNumber;
    lastAlternate = alternate;
    return operationResult;
}

int OpenDevice(libusb_device* device, libusb_device_handle** handle) {
    if (device != fixtureDevice) std::abort();
    if (deviceProbe) deviceProbe();
    *handle = nullptr;
    if (openFails) return LIBUSB_ERROR_ACCESS;
    *handle = reinterpret_cast<libusb_device_handle*>(&handleTokens[openCalls++ % handleTokens.size()]);
    ++hostReferences;
    return 0;
}

ssize_t ListDevices(libusb_context*, libusb_device*** output) {
    static libusb_device* devices[2];
    devices[0] = fixtureDevice;
    devices[1] = nullptr;
    *output = devices;
    ++hostReferences;
    return 1;
}

void FreeDevices(libusb_device**, int unref) {
    if (unref) --hostReferences;
}

int DeviceDescriptor(libusb_device*, libusb_device_descriptor* descriptor) {
    *descriptor = {};
    descriptor->idVendor = 0x1234;
    descriptor->idProduct = 0xabcd;
    return 0;
}

libusb_device* HandleDevice(libusb_device_handle*) {
    return fixtureDevice;
}

void CloseDevice(libusb_device_handle*) {
    ++closeCalls;
    --hostReferences;
}

libusb_device* ReferenceDevice(libusb_device* device) {
    ++hostReferences;
    return device;
}

void UnreferenceDevice(libusb_device*) {
    --hostReferences;
}

void ExitContext(libusb_context*) {
    ++contextExits;
}

int QuerySpeed(libusb_device*) {
    if (deviceProbe) deviceProbe();
    if (deviceThrows) throw std::runtime_error("backend device failure");
    return hostSpeed;
}

int QueryConfiguration(libusb_device*, libusb_config_descriptor** output) {
    ++descriptorReads;
    if (deviceProbe) deviceProbe();
    if (descriptorError != 0) return descriptorError;
    *output = &configuration;
    return 0;
}

void FreeConfiguration(libusb_config_descriptor* descriptor) {
    if (descriptor != &configuration) std::abort();
    ++descriptorFrees;
}

}

#define libusb_get_device_speed QuerySpeed
#define libusb_get_active_config_descriptor QueryConfiguration
#define libusb_free_config_descriptor FreeConfiguration
#define libusb_open OpenDevice
#define libusb_get_device_list ListDevices
#define libusb_free_device_list FreeDevices
#define libusb_get_device_descriptor DeviceDescriptor
#define libusb_get_device HandleDevice
#define libusb_close CloseDevice
#define libusb_ref_device ReferenceDevice
#define libusb_unref_device UnreferenceDevice
#define libusb_exit ExitContext
#define libusb_clear_halt ClearHalt
#define libusb_detach_kernel_driver DetachDriver
#define libusb_set_interface_alt_setting SetAlternate
#define libusb_handle_events PumpEvents
#define libusb_handle_events_timeout_completed PumpEventsTimeout
#include "prx/libSceUsbd/Export.cpp"
#undef libusb_get_device_speed
#undef libusb_get_active_config_descriptor
#undef libusb_free_config_descriptor
#undef libusb_open
#undef libusb_get_device_list
#undef libusb_free_device_list
#undef libusb_get_device_descriptor
#undef libusb_get_device
#undef libusb_close
#undef libusb_ref_device
#undef libusb_unref_device
#undef libusb_exit
#undef libusb_clear_halt
#undef libusb_detach_kernel_driver
#undef libusb_set_interface_alt_setting
#undef libusb_handle_events
#undef libusb_handle_events_timeout_completed

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* function) {
    throw std::runtime_error(function);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD packet queries: %s\n", message);
        std::abort();
    }
}

template<typename TCall>
void RequireThrow(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "unowned or null pointer did not throw");
}

void Speed(void* device) {
    Require(sceUsbdGetDeviceSpeed(nullptr) == 0, "null speed is not unknown");
    for (int speed = 0; speed <= 4; ++speed) {
        hostSpeed = speed;
        Require(sceUsbdGetDeviceSpeed(device) == speed, "known speed changed");
    }
    for (const int speed : {5, 6}) {
        hostSpeed = speed;
        Require(sceUsbdGetDeviceSpeed(device) == 4, "new host speed did not clamp to guest SuperSpeed");
    }
    Require(descriptorReads == 0, "speed queried a configuration");
}

void Packets(void* device) {
    std::array<libusb_endpoint_descriptor, 3> endpoints{};
    endpoints[0].bEndpointAddress = 0x01;
    endpoints[0].wMaxPacketSize = 64;
    endpoints[1].bEndpointAddress = 0x81;
    endpoints[1].wMaxPacketSize = 0x1400;
    endpoints[2].bEndpointAddress = 0x81;
    endpoints[2].wMaxPacketSize = 8;
    std::array<libusb_interface_descriptor, 3> alternates{};
    alternates[0].bNumEndpoints = 1;
    alternates[0].endpoint = &endpoints[0];
    alternates[1].bAlternateSetting = 7;
    alternates[1].bNumEndpoints = 1;
    alternates[1].endpoint = &endpoints[1];
    alternates[2].bNumEndpoints = 1;
    alternates[2].endpoint = &endpoints[2];
    std::array<libusb_interface, 2> interfaces{};
    interfaces[0].num_altsetting = 2;
    interfaces[0].altsetting = alternates.data();
    interfaces[1].num_altsetting = 1;
    interfaces[1].altsetting = &alternates[2];
    configuration.bNumInterfaces = 2;
    configuration.interface = interfaces.data();
    Require(sceUsbdGetMaxPacketSize(device, 0x81) == 0x1400, "first matching alternate or raw packet bits lost");
    Require(sceUsbdGetMaxPacketSize(device, 0x01) == 64, "endpoint direction ignored");
    Require(sceUsbdGetMaxPacketSize(device, 0x82) == static_cast<int>(0x80240005), "missing endpoint not reported");
    alternates[1].bNumEndpoints = 0;
    Require(sceUsbdGetMaxPacketSize(device, 0x81) == 8, "later interface not searched");
    alternates[1].bNumEndpoints = 1;
    for (const int speed : {LIBUSB_SPEED_UNKNOWN, LIBUSB_SPEED_LOW, LIBUSB_SPEED_FULL, LIBUSB_SPEED_HIGH, LIBUSB_SPEED_SUPER, LIBUSB_SPEED_SUPER_PLUS}) {
        hostSpeed = speed;
        for (const auto [raw, highSpeedSize] : std::array<std::pair<int, int>, 5>{{{0, 0}, {0x0400, 1024}, {0x0c00, 2048}, {0x1400, 3072}, {0x1c00, 3072}}}) {
            endpoints[1].wMaxPacketSize = static_cast<std::uint16_t>(raw);
            const int expected = speed == LIBUSB_SPEED_LOW || speed == LIBUSB_SPEED_FULL ? raw : highSpeedSize;
            Require(sceUsbdGetMaxIsoPacketSize(device, 0x81) == expected, "iso bandwidth calculation differs");
        }
    }
    configuration.bNumInterfaces = 0;
    Require(sceUsbdGetMaxPacketSize(device, 0x81) == static_cast<int>(0x80240005), "empty configuration not reported");
    Require(descriptorReads == descriptorFrees, "configuration leaked or freed twice");
}

void Errors(void* device) {
    Require(sceUsbdGetMaxPacketSize(nullptr, 0x81) == static_cast<int>(0x80240004), "null device not mapped to no-device");
    RequireThrow([] { sceUsbdGetMaxIsoPacketSize(nullptr, 0x81); });
    int foreign = 0;
    RequireThrow([&] { sceUsbdGetDeviceSpeed(&foreign); });
    RequireThrow([&] { sceUsbdGetMaxPacketSize(&foreign, 0x81); });
    RequireThrow([&] { sceUsbdGetMaxIsoPacketSize(&foreign, 0x81); });
    Require(descriptorReads == 0, "invalid pointer reached the backend");
    for (const auto [host, guest] : std::array<std::pair<int, std::uint32_t>, 4>{{{-1, 0x80240001}, {-4, 0x80240004}, {-5, 0x80240005}, {-99, 0x802400ff}}}) {
        descriptorError = host;
        Require(sceUsbdGetMaxPacketSize(device, 0x81) == static_cast<int>(guest), "packet error mapping changed");
        Require(sceUsbdGetMaxIsoPacketSize(device, 0x81) == static_cast<int>(guest), "iso error mapping changed");
    }
    Require(descriptorFrees == 0, "failed descriptor request was freed");
    State().devices.clear();
    RequireThrow([&] { sceUsbdGetMaxPacketSize(device, 0x81); });
}

void Lifetime(void* device) {
    hostReferences = 1;
    void* handle = nullptr;
    Require(sceUsbdGetDevice(nullptr) == nullptr, "null handle did not return null");
    int foreign = 0;
    RequireThrow([&] { sceUsbdGetDevice(&foreign); });
    Require(sceUsbdOpen(device, &handle) == 0, "listed device did not open");
    Require(hostReferences == 2, "open did not retain the device");
    sceUsbdUnrefDevice(device);
    Require(State().devices.empty() && hostReferences == 1, "explicit device reference not released");
    Require(sceUsbdGetDevice(handle) == device, "borrowed device identity changed");
    Require(sceUsbdGetDevice(handle) == device && hostReferences == 1, "get-device acquired a reference");
    Require(sceUsbdGetDeviceSpeed(device) == LIBUSB_SPEED_HIGH, "open handle did not preserve device validity");
    RequireThrow([&] { sceUsbdUnrefDevice(device); });
    Require(hostReferences == 1, "borrowed reference was released");
    Require(sceUsbdRefDevice(device) == device && hostReferences == 2, "borrowed device could not be retained");
    sceUsbdClose(handle);
    Require(hostReferences == 1 && closeCalls == 1, "close lost explicit device reference");
    RequireThrow([&] { sceUsbdGetDevice(handle); });
    Require(sceUsbdGetDeviceSpeed(device) == LIBUSB_SPEED_HIGH, "retained device died with handle");
    sceUsbdUnrefDevice(device);
    Require(hostReferences == 0, "retained device leaked");
    RequireThrow([&] { sceUsbdGetDeviceSpeed(device); });
}

void DevicePin(void* device, const char* mode) {
    int token = 0;
    State().context = reinterpret_cast<libusb_context*>(&token);
    State().initCount = 1;
    hostReferences = 1;
    void* borrowedHandle = nullptr;
    const bool borrowed = std::strcmp(mode, "pin-borrowed") == 0;
    if (borrowed) {
        Require(sceUsbdOpen(device, &borrowedHandle) == 0, "borrowed pin fixture did not open");
        sceUsbdUnrefDevice(device);
    }
    static void* closingHandle = nullptr;
    closingHandle = borrowedHandle;
    deviceProbe = [] {
        deviceProbe = nullptr;
        std::thread release([] {
            if (closingHandle) sceUsbdClose(closingHandle);
            else sceUsbdUnrefDevice(fixtureDevice);
            Require(hostReferences == 1, "device was released while the backend still uses it");
            RequireThrow([] { sceUsbdExit(); });
            Require(State().initCount == 1 && contextExits == 0, "active query lost its context");
        });
        release.join();
    };
    if (std::strcmp(mode, "pin-throw") == 0) {
        deviceThrows = true;
        RequireThrow([&] { sceUsbdGetDeviceSpeed(device); });
        deviceThrows = false;
    } else if (std::strcmp(mode, "pin-error") == 0) {
        descriptorError = LIBUSB_ERROR_IO;
        Require(sceUsbdGetMaxPacketSize(device, 0x81) == static_cast<int>(0x80240001), "pinned query error changed");
    } else if (std::strcmp(mode, "pin-iso") == 0) {
        configuration.bNumInterfaces = 0;
        Require(sceUsbdGetMaxIsoPacketSize(device, 0x81) == static_cast<int>(0x80240005), "pinned iso query changed");
    } else if (std::strcmp(mode, "pin-open") == 0) {
        void* handle = nullptr;
        Require(sceUsbdOpen(device, &handle) == 0 && hostReferences == 1, "opening lost its retained device");
        sceUsbdClose(handle);
    } else {
        Require(sceUsbdGetDeviceSpeed(device) == hostSpeed, "pinned speed changed");
    }
    Require(hostReferences == 0 && State().devices.empty() && State().handles.empty(), "query reference leaked");
    RequireThrow([&] { sceUsbdGetDeviceSpeed(device); });
    sceUsbdExit();
    Require(State().initCount == 0 && contextExits == 1, "context could not exit after query completion");
}

void OpenById(void* device) {
    State().devices.clear();
    Require(sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd) == nullptr, "no-context open reached backend");
    int contextToken = 0;
    State().context = reinterpret_cast<libusb_context*>(&contextToken);
    Require(sceUsbdOpenDeviceWithVidPid(0x1235, 0xabcd) == nullptr, "wrong vendor matched");
    Require(sceUsbdOpenDeviceWithVidPid(0x1234, 0xabce) == nullptr, "wrong product matched");
    openFails = true;
    Require(sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd) == nullptr, "failed open returned a handle");
    Require(State().handles.empty() && hostReferences == 0, "failed open leaked ownership");
    openFails = false;
    void* first = sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd);
    void* second = sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd);
    Require(first != nullptr && second != nullptr && first != second, "matching opens did not return distinct handles");
    Require(sceUsbdGetDevice(first) == device && sceUsbdGetDevice(second) == device, "matching handle device not tracked");
    Require(State().devices.empty() && hostReferences == 2, "open-by-id added an explicit reference");
    sceUsbdClose(first);
    Require(sceUsbdGetDeviceSpeed(device) == LIBUSB_SPEED_HIGH, "closing one handle invalidated another");
    RequireThrow([&] { sceUsbdClose(first); });
    sceUsbdClose(second);
    Require(hostReferences == 0 && closeCalls == 2, "matching handles leaked references");
    RequireThrow([&] { sceUsbdGetDeviceSpeed(device); });
    State().context = nullptr;
}

void ExitWithHandles(void* device) {
    int contextToken = 0;
    State().context = reinterpret_cast<libusb_context*>(&contextToken);
    State().initCount = 2;
    hostReferences = 1;
    void* first = sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd);
    void* second = sceUsbdOpenDeviceWithVidPid(0x1234, 0xabcd);
    Require(first != nullptr && second != nullptr && hostReferences == 3, "exit fixture did not open handles");
    sceUsbdExit();
    Require(hostReferences == 3 && contextExits == 0, "nested initialization exited too early");
    Require(sceUsbdGetDevice(first) == device, "nested exit invalidated a handle");
    sceUsbdExit();
    Require(hostReferences == 0 && closeCalls == 2 && contextExits == 1, "final exit did not release handles and explicit references once");
    Require(State().handles.empty() && State().devices.empty() && State().context == nullptr, "exit retained ownership records");
    RequireThrow([&] { sceUsbdGetDevice(first); });
    RequireThrow([&] { sceUsbdGetDevice(second); });
    RequireThrow([&] { sceUsbdGetDeviceSpeed(device); });
    sceUsbdExit();
    Require(contextExits == 1, "repeated exit closed the context twice");
}

void Operations(void* device) {
    constexpr int invalid = static_cast<int>(0x80240002);
    RequireThrow([] { sceUsbdClearHalt(nullptr, 0x81); });
    Require(sceUsbdDetachKernelDriver(nullptr, 0) == invalid, "null detach accepted");
    Require(sceUsbdSetInterfaceAltSetting(nullptr, 0, 0) == invalid, "null alternate accepted");
    int foreign = 0;
    RequireThrow([&] { sceUsbdClearHalt(&foreign, 0x81); });
    RequireThrow([&] { sceUsbdDetachKernelDriver(&foreign, 0); });
    RequireThrow([&] { sceUsbdSetInterfaceAltSetting(&foreign, 0, 0); });
    Require(operationCalls == 0, "invalid handle reached device-changing backend");
    void* handle = nullptr;
    Require(sceUsbdOpen(device, &handle) == 0, "operation handle open failed");
    Require(sceUsbdSetInterfaceAltSetting(handle, 31, 7) == static_cast<int>(0x80240005), "unclaimed interface reached backend");
    State().claimed[static_cast<libusb_device_handle*>(handle)] = (1u << 31) | (1u << 1);
    for (const int error : {0, -1, -3, -4, -5, -6, -9, -12, -99}) {
        operationResult = error;
        const int expected = error == 0 ? 0 : static_cast<int>(0x802400ffu);
        if (error == 0 || error == LIBUSB_ERROR_NOT_FOUND) Require(sceUsbdClearHalt(handle, 0x83) == expected, "clear-halt known result differs");
        else RequireThrow([&] { sceUsbdClearHalt(handle, 0x83); });
        Require(lastEndpoint == 0x83, "clear-halt endpoint changed");
        Require(sceUsbdDetachKernelDriver(handle, 17) == expected && lastInterface == 17, "detach forwarding differs");
        Require(sceUsbdSetInterfaceAltSetting(handle, 31, 7) == expected && lastInterface == 31 && lastAlternate == 7, "alternate forwarding differs");
    }
    const int calls = operationCalls;
    Require(sceUsbdSetInterfaceAltSetting(handle, -1, 0) == invalid, "negative interface accepted");
    Require(sceUsbdSetInterfaceAltSetting(handle, 32, 0) == invalid, "oversized interface accepted");
    Require(operationCalls == calls, "invalid interface reached backend");
    UsbdTransfer pending{};
    pending.deviceHandle = handle;
    libusb_transfer nativePending{};
    nativePending.dev_handle = static_cast<libusb_device_handle*>(handle);
    State().inFlight.emplace(&pending, &nativePending);
    RequireThrow([&] { sceUsbdSetInterfaceAltSetting(handle, 1, 2); });
    Require(operationCalls == calls, "pending transfer switch silently reached backend");
    State().inFlight.clear();
    sceUsbdClose(handle);
    RequireThrow([&] { sceUsbdClearHalt(handle, 0x83); });
}

void ExitPending() {
    State().initCount = 1;
    UsbdTransfer transfer{};
    State().inFlight.emplace(&transfer, nullptr);
    RequireThrow([] { sceUsbdExit(); });
    Require(State().initCount == 1, "rejected shutdown consumed initialization reference");
    State().inFlight.clear();
    State().devices.clear();
    sceUsbdExit();
    Require(State().initCount == 0, "shutdown could not be retried after completion");
}

void ClosePending(void* device) {
    void* handle = nullptr;
    void* other = nullptr;
    Require(sceUsbdOpen(device, &handle) == 0 && sceUsbdOpen(device, &other) == 0, "pending-close fixture open failed");
    UsbdTransfer transfer{};
    transfer.deviceHandle = handle;
    State().inFlight.emplace(&transfer, nullptr);
    RequireThrow([&] { sceUsbdClose(handle); });
    Require(sceUsbdGetDevice(handle) == device, "rejected close erased handle ownership");
    sceUsbdClose(other);
    Require(State().inFlight.contains(&transfer), "unrelated close discarded pending transfer");
    State().inFlight.clear();
    sceUsbdClose(handle);
    RequireThrow([&] { sceUsbdGetDevice(handle); });
}

void EventDispatch() {
    int token = 0;
    State().context = reinterpret_cast<libusb_context*>(&token);
    State().initCount = 1;
    eventProbe = [] {
        Require(sceUsbdEventHandlingOk() == 1, "callback lost handler ownership");
        sceUsbdLockEventWaiters();
        sceUsbdUnlockEventWaiters();
        Events().completions.fetch_add(1);
    };
    UsbdTimeval timeout{12, 345678};
    for (const auto [host, guest] : std::array<std::pair<int, std::uint32_t>, 4>{{{0, 0}, {-7, 0x80240007}, {-10, 0x8024000a}, {-99, 0x802400ff}}}) {
        eventResult = host;
        Require(sceUsbdHandleEvents() == static_cast<int>(guest) && eventTimeout.tv_sec == 1, "ordinary event error forwarding changed");
        Require(sceUsbdEventHandlingOk() == 0 && sceUsbdEventHandlerActive() == 0, "ordinary pump retained handler ownership");
        Require(sceUsbdHandleEventsTimeout(&timeout) == static_cast<int>(guest), "timed event error forwarding changed");
        Require(!unboundedEvent && eventTimeout.tv_sec == 12 && eventTimeout.tv_usec == 345678, "event timeout ABI conversion failed");
        sceUsbdLockEvents();
        Require(sceUsbdHandleEventsLocked(&timeout) == static_cast<int>(guest), "locked event result changed");
        Require(sceUsbdEventHandlingOk() == 1, "locked pump released ownership");
        RequireThrow([] { sceUsbdExit(); });
        Require(State().initCount == 1, "active-handler shutdown consumed init reference");
        sceUsbdUnlockEvents();
    }
    eventThrows = true;
    RequireThrow([] { sceUsbdHandleEvents(); });
    Require(sceUsbdTryLockEvents() == 0, "exception leaked guest event lock");
    sceUsbdUnlockEvents();
    eventThrows = false;
    Require(eventCalls == 13, "event backend dispatch count changed");
    State().devices.clear();
    sceUsbdExit();
    Require(contextExits == 1, "event fixture failed to exit");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected a query or ownership case");
    int token = 0;
    auto* device = reinterpret_cast<libusb_device*>(&token);
    fixtureDevice = device;
    State().devices.emplace(device, 1);
    if (std::strcmp(argv[1], "speed") == 0) Speed(device);
    else if (std::strcmp(argv[1], "packets") == 0) Packets(device);
    else if (std::strcmp(argv[1], "errors") == 0) Errors(device);
    else if (std::strcmp(argv[1], "lifetime") == 0) Lifetime(device);
    else if (std::strcmp(argv[1], "open-by-id") == 0) OpenById(device);
    else if (std::strcmp(argv[1], "exit") == 0) ExitWithHandles(device);
    else if (std::strcmp(argv[1], "operations") == 0) Operations(device);
    else if (std::strcmp(argv[1], "exit-pending") == 0) ExitPending();
    else if (std::strcmp(argv[1], "close-pending") == 0) ClosePending(device);
    else if (std::strcmp(argv[1], "event-dispatch") == 0) EventDispatch();
    else if (std::strncmp(argv[1], "pin-", 4) == 0) DevicePin(device, argv[1]);
    else Require(false, "unknown test case");
    State().devices.clear();
    std::puts("USBD packet query tests passed");
}
