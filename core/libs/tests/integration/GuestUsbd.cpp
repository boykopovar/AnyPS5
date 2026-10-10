#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

} // namespace

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
int APS5_VABI sceUsbdOpen();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);

class UsbdSession {
public:
    UsbdSession() {
        RequireEqual(sceUsbdInit(), 0, "initialization");
    }

    ~UsbdSession() {
        sceUsbdExit();
    }

    UsbdSession(const UsbdSession&) = delete;
    UsbdSession& operator=(const UsbdSession&) = delete;
};

const Case deviceList{"GetDeviceList_NoDevices_ReturnsEmptyNullTerminatedList", [] {
    const UsbdSession session;
    void** list = nullptr;
    RequireEqual(sceUsbdGetDeviceList(&list), std::int64_t{0}, "listed device count");
    Require(list != nullptr, "the device list is null");
    Require(list[0] == nullptr, "the device list is not null-terminated at index 0");
    sceUsbdFreeDeviceList(list, 1);
}};

const Case deviceListNull{"GetDeviceList_NullOutput_ReturnsInvalidArgument", [] {
    const UsbdSession session;
    RequireEqual(sceUsbdGetDeviceList(nullptr), std::int64_t{invalidArgument}, "get device list with a null output");
}};

const Case eventsNullTimeout{"HandleEventsTimeout_NullTimeout_ReturnsInvalidArgument", [] {
    const UsbdSession session;
    RequireEqual(sceUsbdHandleEventsTimeout(nullptr), invalidArgument, "null timeout");
}};

const Case eventsInvalidTimeout{"HandleEventsTimeout_MicrosecondsOutOfRange_ReturnsInvalidArgument", [] {
    const UsbdSession session;
    const UsbdTimeval invalid{0, 1000000};
    RequireEqual(sceUsbdHandleEventsTimeout(&invalid), invalidArgument, "one million microseconds");
}};

const Case eventsWait{"HandleEventsTimeout_FiftyMilliseconds_WaitsForTheTimeout", [] {
    const UsbdSession session;
    const UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    RequireEqual(sceUsbdHandleEventsTimeout(&timeout), 0, "event handling");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");
}};

const Case open{"Open_NoDevice_ThrowsRuntimeError", [] {
    const UsbdSession session;
    RequireThrows<std::runtime_error>([] { sceUsbdOpen(); }, "opening a device");
}};

} // namespace
