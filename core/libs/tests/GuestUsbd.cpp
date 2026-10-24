#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <stdexcept>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
std::int32_t APS5_VABI sceUsbdOpen(void* device, void** deviceHandle);
void APS5_VABI sceUsbdClose(void* deviceHandle);
void* APS5_VABI sceUsbdRefDevice(void* device);
void APS5_VABI sceUsbdUnrefDevice(void* device);
std::uint8_t APS5_VABI sceUsbdGetBusNumber(void* device);
std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(void* device);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config);
std::int32_t APS5_VABI sceUsbdGetActiveConfigDescriptor(void* device, void** config);
std::int32_t APS5_VABI sceUsbdClaimInterface(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdReleaseInterface(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdSetConfiguration(void* deviceHandle, std::int32_t configuration);
std::int32_t APS5_VABI sceUsbdResetDevice(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdCheckConnected(void* deviceHandle);
std::int32_t APS5_VABI sceUsbdKernelDriverActive(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdAttachKernelDriver(void* deviceHandle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdControlTransfer(void* deviceHandle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint8_t* data, std::uint16_t length, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdGetStringDescriptor(void* deviceHandle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* data, std::int32_t length);
std::int32_t APS5_VABI sceUsbdGetStringDescriptorAscii(void* deviceHandle, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length);
std::int32_t APS5_VABI sceUsbdGetDescriptor(void* deviceHandle, std::uint8_t descriptorType, std::uint8_t descriptorIndex, std::uint8_t* data, std::int32_t length);
std::int32_t APS5_VABI sceUsbdGetConfiguration(void* deviceHandle, std::int32_t* configuration);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptorByValue(void* device, std::uint8_t configurationValue, void** config);
std::int32_t APS5_VABI sceUsbdClearHalt(void* deviceHandle, std::uint8_t endpoint);
std::int32_t APS5_VABI sceUsbdDetachKernelDriver(void* deviceHandle, int interfaceNumber);
void* APS5_VABI sceUsbdGetDevice(void* deviceHandle);
int APS5_VABI sceUsbdGetDeviceSpeed(void* device);
std::int32_t APS5_VABI sceUsbdGetMaxIsoPacketSize(void* device, std::uint8_t endpoint);
std::int32_t APS5_VABI sceUsbdGetMaxPacketSize(void* device, std::uint8_t endpoint);
void* APS5_VABI sceUsbdOpenDeviceWithVidPid(std::uint16_t vendorId, std::uint16_t productId);
std::int32_t APS5_VABI sceUsbdSetInterfaceAltSetting(void* deviceHandle, int interfaceNumber, int alternateSetting);
std::int32_t APS5_VABI sceUsbdEventHandlerActive();
std::int32_t APS5_VABI sceUsbdHandleEvents();
std::int32_t APS5_VABI sceUsbdHandleEventsLocked(UsbdTimeval* timeout);
void APS5_VABI sceUsbdLockEventWaiters();
void APS5_VABI sceUsbdLockEvents();
std::int32_t APS5_VABI sceUsbdTryLockEvents();
void APS5_VABI sceUsbdUnlockEventWaiters();
void APS5_VABI sceUsbdUnlockEvents();
std::int32_t APS5_VABI sceUsbdWaitForEvent(UsbdTimeval* timeout);
std::int32_t APS5_VABI sceUsbdSubmitTransfer(void* transfer);
void* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets);
void APS5_VABI sceUsbdFreeTransfer(void* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(void* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, void* callback, void* userData, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdCancelTransfer(void* transfer);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdFreeConfigDescriptor(void* config);
void APS5_VABI sceUsbdFillControlSetup(std::uint8_t* buffer, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint16_t length);
}

namespace {

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t noDevice = static_cast<std::int32_t>(0x80240004);
constexpr std::int32_t notFound = static_cast<std::int32_t>(0x80240005);
constexpr std::int32_t otherError = static_cast<std::int32_t>(0x802400FF);
constexpr std::uint8_t absentEndpoint = 0x70;
constexpr std::int32_t timedOut = static_cast<std::int32_t>(0x80240007);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct HostTransfer {
    void* deviceHandle;
    std::uint8_t flags;
    std::uint8_t endpoint;
    std::uint8_t type;
    std::uint32_t timeout;
    std::int32_t status;
    std::int32_t length;
    std::int32_t actualLength;
    void (APS5_VABI* callback)(HostTransfer*);
    void* userData;
    std::uint8_t* buffer;
    std::int32_t numIsoPackets;
};

void APS5_VABI MarkDone(HostTransfer* transfer) {
    *static_cast<int*>(transfer->userData) += 1;
}

void APS5_VABI MarkDoneThenSignal(HostTransfer* transfer) {
    auto* notified = static_cast<std::atomic<bool>*>(transfer->userData);
    notified->store(true, std::memory_order_release);
}

void EventLocks() {
    Require(sceUsbdEventHandlerActive() == 0 && sceUsbdEventHandlingOk() == 0, "an event handler was active before any lock");
    sceUsbdLockEvents();
    Require(sceUsbdEventHandlingOk() == 1 && sceUsbdEventHandlerActive() == 1, "the thread that locked events is not the handler");
    Require(sceUsbdTryLockEvents() == 1, "the holder of the event lock took it again");
    Require(Throws([] { sceUsbdLockEvents(); }), "one thread took the event lock twice");
    Require(Throws([] { sceUsbdLockEventWaiters(); }), "the holder of the event lock took it again as a waiter");
    std::thread([] {
        Require(sceUsbdTryLockEvents() == 1, "the event lock was taken while another thread held it");
        Require(sceUsbdEventHandlingOk() == 0 && sceUsbdEventHandlerActive() == 1, "a thread without the lock saw itself as the handler");
    }).join();
    sceUsbdUnlockEvents();
    Require(sceUsbdEventHandlerActive() == 0 && sceUsbdEventHandlingOk() == 0, "the handler stayed active after unlocking events");
    UsbdTimeval brief{0, 20000};
    Require(Throws([] { sceUsbdUnlockEvents(); }) && Throws([] { sceUsbdUnlockEventWaiters(); }), "an event lock that was not held was released");
    Require(Throws([&] { sceUsbdWaitForEvent(&brief); }) && Throws([&] { sceUsbdHandleEventsLocked(&brief); }), "waited or handled events without the event lock");
    Require(sceUsbdTryLockEvents() == 0 && sceUsbdEventHandlingOk() == 1, "a free event lock was not taken");
    UsbdTimeval negative{-1, 0};
    Require(Throws([&] { sceUsbdHandleEventsLocked(&negative); }) && Throws([&] { sceUsbdWaitForEvent(&negative); }), "a negative timeout was accepted");
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsLocked(&brief) == 0, "the handler failed to handle events");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15), "the handler returned before the timeout");
    sceUsbdUnlockEvents();

    sceUsbdLockEventWaiters();
    Require(sceUsbdEventHandlingOk() == 0, "locking the event waiters made this thread the handler");
    Require(sceUsbdWaitForEvent(&brief) == 1, "waiting without a handler did not time out");
    Require(sceUsbdEventHandlingOk() == 1, "a waiter did not take over handling after a timeout with no handler");
    sceUsbdUnlockEvents();
    Require(sceUsbdEventHandlerActive() == 0, "the waiter kept handling after unlocking events");

    sceUsbdLockEvents();
    std::atomic<bool> waiting{false};
    std::atomic<int> woken{-1};
    std::atomic<int> handling{-1};
    std::thread waiter([&] {
        sceUsbdLockEventWaiters();
        UsbdTimeval patience{5, 0};
        waiting.store(true);
        woken.store(sceUsbdWaitForEvent(&patience));
        handling.store(sceUsbdEventHandlingOk());
        sceUsbdUnlockEvents();
    });
    while (!waiting.load()) Require(sceUsbdHandleEventsLocked(&brief) == 0, "the handler failed while a waiter queued");
    sceUsbdUnlockEvents();
    waiter.join();
    Require(woken.load() == 0 && handling.load() == 1, "a waiter was not woken and handed the events when the handler unlocked");
    Require(sceUsbdEventHandlerActive() == 0, "the handler stayed active after the woken waiter unlocked");

    sceUsbdLockEvents();
    std::atomic<bool> entered{false};
    std::atomic<std::int32_t> waiterResult{0};
    std::thread follower([&] {
        sceUsbdLockEventWaiters();
        entered.store(true);
        UsbdTimeval shortWait{0, 30000};
        waiterResult.store(sceUsbdHandleEventsLocked(&shortWait));
        sceUsbdUnlockEventWaiters();
    });
    while (!entered.load()) Require(sceUsbdHandleEventsLocked(&brief) == 0, "the handler failed while another thread queued");
    UsbdTimeval longer{0, 100000};
    Require(sceUsbdHandleEventsLocked(&longer) == 0, "the handler failed while another thread waited");
    follower.join();
    Require(waiterResult.load() == timedOut, "a thread that is not the handler did not time out in sceUsbdHandleEventsLocked");
    sceUsbdUnlockEvents();
    Require(sceUsbdEventHandlerActive() == 0, "the handler stayed active at the end");
}

void Pump(const int& done) {
    const UsbdTimeval slice{0, 100000};
    for (int round = 0; round < 50 && done == 0; ++round) Require(sceUsbdHandleEventsTimeout(&slice) == 0, "event handling failed while a transfer was in flight");
}

void Device() {
    const char* wanted = std::getenv("APS5_USBD_DEVICE");
    Require(wanted != nullptr, "APS5_USBD_DEVICE is not set to vendor:product in hex");
    const unsigned long vendor = std::strtoul(wanted, nullptr, 16);
    const char* separator = std::strchr(wanted, ':');
    Require(separator != nullptr, "APS5_USBD_DEVICE has no ':'");
    const unsigned long product = std::strtoul(separator + 1, nullptr, 16);
    void** list = nullptr;
    const std::int64_t count = sceUsbdGetDeviceList(&list);
    void* device = nullptr;
    std::uint8_t descriptor[18] = {};
    for (std::int64_t index = 0; index < count && device == nullptr; ++index) {
        Require(sceUsbdGetDeviceDescriptor(list[index], descriptor) == 0, "device descriptor failed");
        if ((descriptor[8] | descriptor[9] << 8) == vendor && (descriptor[10] | descriptor[11] << 8) == product) device = sceUsbdRefDevice(list[index]);
    }
    sceUsbdFreeDeviceList(list, 1);
    Require(device != nullptr, "the APS5_USBD_DEVICE device was not listed");
    std::printf("device bus %u address %u\n", sceUsbdGetBusNumber(device), sceUsbdGetDeviceAddress(device));
    void* config = nullptr;
    Require(sceUsbdGetActiveConfigDescriptor(device, &config) == 0 && config != nullptr, "active config descriptor failed");
    const auto* raw = static_cast<const std::uint8_t*>(config);
    Require(raw[1] == 2 && raw[4] > 0, "active config descriptor is not a configuration with interfaces");
    const std::uint8_t interfaces = raw[4];
    const std::uint8_t activeValue = raw[5];
    sceUsbdFreeConfigDescriptor(config);
    Require(Throws([&] { sceUsbdFreeConfigDescriptor(config); }), "a config descriptor was freed twice");
    Require(sceUsbdGetConfigDescriptor(device, 0, &config) == 0, "config descriptor 0 failed");
    sceUsbdFreeConfigDescriptor(config);
    Require(sceUsbdGetConfigDescriptorByValue(device, activeValue, &config) == 0 && config != nullptr, "config descriptor by value failed");
    Require(static_cast<const std::uint8_t*>(config)[5] == activeValue, "config descriptor by value has another configuration value");
    sceUsbdFreeConfigDescriptor(config);
    Require(sceUsbdGetConfigDescriptorByValue(device, 0, &config) == static_cast<std::int32_t>(0x80240005) && config == nullptr, "configuration value 0 was found");
    void* handle = nullptr;
    Require(sceUsbdOpen(device, &handle) == 0 && handle != nullptr, "opening the device failed");
    Require(sceUsbdCheckConnected(handle) == 0, "an open device was reported disconnected");
    Require(sceUsbdGetDevice(handle) == device, "the device of an open handle is not the device it was opened from");
    const int speed = sceUsbdGetDeviceSpeed(device);
    Require(speed >= 1 && speed <= 4, "the device speed is not low, full, high or super speed");
    std::printf("device speed %d\n", speed);
    std::int32_t configuration = -1;
    Require(sceUsbdGetConfiguration(handle, &configuration) == 0 && configuration == activeValue, "configuration differs from the active config descriptor");
    std::uint8_t text[256] = {};
    if (descriptor[15] != 0) {
        const std::int32_t length = sceUsbdGetStringDescriptor(handle, descriptor[15], 0x409, text, sizeof(text));
        Require(length >= 2 && text[1] == 3, "product string descriptor failed");
        std::string product_name;
        for (std::int32_t index = 2; index + 1 < length; index += 2) product_name += static_cast<char>(text[index]);
        std::printf("product \"%s\"\n", product_name.c_str());
        char ascii[256] = {};
        const std::int32_t asciiLength = sceUsbdGetStringDescriptorAscii(handle, descriptor[15], reinterpret_cast<std::uint8_t*>(ascii), sizeof(ascii));
        Require(asciiLength >= 0 && std::strlen(ascii) == static_cast<std::size_t>(asciiLength), "ASCII product string length differs from its terminated length");
        std::printf("ASCII product \"%s\"\n", ascii);
        std::uint8_t languages[4] = {};
        Require(sceUsbdGetStringDescriptor(handle, 0, 0, languages, sizeof(languages)) == 4, "language ID string descriptor failed");
        std::uint8_t first[256] = {};
        const std::int32_t firstLength = sceUsbdGetStringDescriptor(handle, descriptor[15], static_cast<std::uint16_t>(languages[2] | languages[3] << 8), first, sizeof(first));
        Require(firstLength >= 2 && first[1] == 3, "product string descriptor in the first language failed");
        const std::int32_t end = first[0] < firstLength ? first[0] : firstLength;
        std::string printable;
        for (std::int32_t index = 2; index + 1 < end; index += 2) {
            if (first[index + 1] == 0 && first[index] >= 0x20 && first[index] <= 0x7E && first[index] != '<' && first[index] != '>') printable += static_cast<char>(first[index]);
        }
        Require(printable == ascii, "ASCII product string differs from the printable characters of the product string");
        char shortAscii[4] = {'x', 'x', 'x', 'x'};
        const std::int32_t shortLength = sceUsbdGetStringDescriptorAscii(handle, descriptor[15], reinterpret_cast<std::uint8_t*>(shortAscii), sizeof(shortAscii));
        Require(shortLength >= 0 && shortLength <= 3 && shortAscii[shortLength] == 0 && std::strncmp(shortAscii, ascii, static_cast<std::size_t>(shortLength)) == 0, "a short ASCII buffer was not filled with a terminated prefix");
    }
    std::uint8_t again[18] = {};
    Require(sceUsbdControlTransfer(handle, 0x80, 6, 0x0100, 0, again, sizeof(again), 1000) == 18, "synchronous device descriptor request failed");
    Require(std::memcmp(again, descriptor, sizeof(again)) == 0, "device descriptor read over the wire differs from the cached one");
    std::uint8_t wire[18] = {};
    Require(sceUsbdGetDescriptor(handle, 1, 0, wire, sizeof(wire)) == 18, "device descriptor request through sceUsbdGetDescriptor failed");
    Require(std::memcmp(wire, descriptor, sizeof(wire)) == 0, "device descriptor read through sceUsbdGetDescriptor differs from the cached one");
    for (std::int32_t interfaceNumber = 0; interfaceNumber < interfaces; ++interfaceNumber) {
        Require(sceUsbdClaimInterface(handle, interfaceNumber) == 0, "claiming an interface failed");
        Require(sceUsbdReleaseInterface(handle, interfaceNumber) == 0, "releasing an interface failed");
    }
    Require(sceUsbdReleaseInterface(handle, 0) == static_cast<std::int32_t>(0x80240005), "releasing an unclaimed interface did not report not found");
    Require(sceUsbdGetConfigDescriptor(device, 0, &config) == 0, "config descriptor 0 for the endpoint scan failed");
    const std::uint8_t* config0 = static_cast<const std::uint8_t*>(config);
    const std::uint8_t* interfaceArray = *reinterpret_cast<const std::uint8_t* const*>(config0 + 16);
    std::uint8_t interruptAddress = 0;
    std::uint16_t interruptMax = 0;
    std::uint8_t interruptInterface = 0;
    if (interfaceArray == nullptr) {
        Require(false, "the active config descriptor has no interface array");
    }
    for (std::uint8_t index = 0; index < interfaces && interruptAddress == 0; ++index) {
        const std::uint8_t* alternate = *reinterpret_cast<const std::uint8_t* const*>(interfaceArray + index * 16);
        if (alternate == nullptr) continue;
        const std::uint8_t* endpoints = *reinterpret_cast<const std::uint8_t* const*>(alternate + 16);
        if (endpoints == nullptr) continue;
        for (std::uint8_t endpoint = 0; endpoint < alternate[4]; ++endpoint) {
            const std::uint8_t* item = endpoints + endpoint * 32;
            if ((item[2] & 0x80) != 0 && (item[3] & 3) == 3) {
                interruptAddress = item[2];
                interruptMax = static_cast<std::uint16_t>(item[4] | item[5] << 8);
                interruptInterface = alternate[2];
            }
        }
    }
    Require(interruptAddress != 0 && interruptMax != 0, "the device has no interrupt IN endpoint");
    sceUsbdFreeConfigDescriptor(config);
    Require(sceUsbdGetMaxPacketSize(device, interruptAddress) == interruptMax, "the max packet size differs from the endpoint descriptor");
    const int expectedIso = speed <= 2 ? interruptMax : (interruptMax & 0x7FF) * std::min(1 + ((interruptMax >> 11) & 3), 3);
    Require(sceUsbdGetMaxIsoPacketSize(device, interruptAddress) == expectedIso, "the max iso packet size differs from the endpoint descriptor");
    Require(sceUsbdGetMaxPacketSize(device, absentEndpoint) == notFound && sceUsbdGetMaxIsoPacketSize(device, absentEndpoint) == notFound, "an absent endpoint has a max packet size");
    Require(sceUsbdSetInterfaceAltSetting(handle, interruptInterface, 0) == notFound, "an unclaimed interface took an alternate setting");
    Require(sceUsbdSetInterfaceAltSetting(handle, 32, 0) == invalidArgument && sceUsbdSetInterfaceAltSetting(handle, -1, 0) == invalidArgument, "an interface number outside 0 to 31 was accepted");
    Require(sceUsbdClaimInterface(handle, interruptInterface) == 0, "claiming the interrupt endpoint's interface failed");
    const std::int32_t alternate = sceUsbdSetInterfaceAltSetting(handle, interruptInterface, 0);
    Require(alternate == 0 || alternate == otherError, "alternate setting 0 returned neither success nor the other error");
    std::printf("alternate setting 0 of interface %u: %d\n", interruptInterface, alternate);
    Require(sceUsbdClearHalt(handle, interruptAddress) == 0, "clearing the interrupt endpoint's halt failed");
    Require(sceUsbdClearHalt(handle, absentEndpoint) == otherError, "clearing the halt of an absent endpoint did not return the other error");
    const std::int32_t detached = sceUsbdDetachKernelDriver(handle, interruptInterface);
    Require(detached == 0 || detached == otherError, "detaching the kernel driver returned neither success nor the other error");
    std::printf("kernel driver detach: %d\n", detached);
    Require(sceUsbdReleaseInterface(handle, interruptInterface) == 0, "releasing the interrupt endpoint's interface failed");
    Require(sceUsbdSetInterfaceAltSetting(handle, interruptInterface, 0) == notFound, "a released interface took an alternate setting");

    auto* transfer = static_cast<HostTransfer*>(sceUsbdAllocTransfer(0));
    std::uint8_t setup[8 + 18] = {0x80, 6, 0x00, 0x01, 0x00, 0x00, 18, 0};
    int done = 0;
    transfer->deviceHandle = handle;
    transfer->type = 0;
    transfer->timeout = 1000;
    transfer->buffer = setup;
    transfer->length = sizeof(setup);
    transfer->callback = MarkDone;
    transfer->userData = &done;
    Require(sceUsbdSubmitTransfer(transfer) == 0, "submitting an asynchronous control transfer failed");
    Require(Throws([&] { sceUsbdFreeTransfer(transfer); }), "an in-flight transfer was freed");
    Pump(done);
    Require(done == 1 && transfer->status == 0 && transfer->actualLength == 18, "asynchronous control transfer did not complete");
    done = 0;
    Require(sceUsbdSubmitTransfer(transfer) == 0, "resubmitting the control transfer failed");
    while (done == 0) Require(sceUsbdHandleEvents() == 0, "sceUsbdHandleEvents failed while a transfer was in flight");
    Require(transfer->status == 0 && transfer->actualLength == 18, "the control transfer did not complete through sceUsbdHandleEvents");
    done = 0;
    sceUsbdLockEvents();
    Require(sceUsbdEventHandlingOk() == 1, "the thread that locked events cannot handle them");
    Require(sceUsbdSubmitTransfer(transfer) == 0, "submitting the control transfer under the event lock failed");
    {
        UsbdTimeval slice{0, 100000};
        for (int round = 0; round < 50 && done == 0; ++round) Require(sceUsbdHandleEventsLocked(&slice) == 0, "sceUsbdHandleEventsLocked failed while a transfer was in flight");
    }
    sceUsbdUnlockEvents();
    Require(done == 1 && transfer->status == 0 && transfer->actualLength == 18, "the control transfer did not complete through sceUsbdHandleEventsLocked");
    Require(std::memcmp(setup + 8, descriptor, 18) == 0, "asynchronous device descriptor differs from the cached one");
    bool broken = false;
    {
        const UsbdTimeval slice{0, 200};
        for (int round = 0; round < 300 && !broken; ++round) {
            std::atomic<bool> notified{false};
            std::uint8_t blink[64] = {};
            sceUsbdFillInterruptTransfer(transfer, handle, interruptAddress, blink, interruptMax, reinterpret_cast<void*>(&MarkDoneThenSignal), &notified, 0);
            const int submitResult = sceUsbdSubmitTransfer(transfer);
            if (submitResult != 0) {
                broken = true;
                break;
            }
            std::thread canceller([&] {
                const int outcome = sceUsbdCancelTransfer(transfer);
                if (outcome != 0 && outcome != static_cast<std::int32_t>(0x80240005)) broken = true;
            });
            while (!notified.load(std::memory_order_acquire)) {
                if (sceUsbdHandleEventsTimeout(&slice) != 0) broken = true;
            }
            canceller.join();
            if (transfer->status != 0 && transfer->status != 2 && transfer->status != 3) broken = true;
        }
    }
    Require(!broken, "the cancel race failed");

    Require(sceUsbdCancelTransfer(transfer) == static_cast<std::int32_t>(0x80240005), "cancelling a completed transfer did not report not found");
    sceUsbdFreeTransfer(transfer);
    sceUsbdClose(handle);
    Require(Throws([&] { sceUsbdClose(handle); }), "a handle was closed twice");
    Require(Throws([&] { sceUsbdGetDevice(handle); }), "the device of a closed handle was returned");
    sceUsbdUnrefDevice(device);
    void* byId = sceUsbdOpenDeviceWithVidPid(static_cast<std::uint16_t>(vendor), static_cast<std::uint16_t>(product));
    Require(byId != nullptr, "opening the device by vendor and product failed");
    void* byIdDevice = sceUsbdGetDevice(byId);
    std::uint8_t byIdDescriptor[18] = {};
    Require(sceUsbdGetDeviceDescriptor(byIdDevice, byIdDescriptor) == 0, "the device of a handle opened by vendor and product has no descriptor");
    Require((byIdDescriptor[8] | byIdDescriptor[9] << 8) == vendor && (byIdDescriptor[10] | byIdDescriptor[11] << 8) == product, "the device opened by vendor and product has another vendor or product");
    Require(sceUsbdGetMaxPacketSize(byIdDevice, interruptAddress) == interruptMax, "the max packet size through a handle's device differs");
    sceUsbdClose(byId);
    Require(Throws([&] { sceUsbdGetDeviceDescriptor(byIdDevice, byIdDescriptor); }), "the device of a closed handle stayed known");
    std::puts("USBD device tests passed");
}

}

int main(int argc, char** argv) {
    Require(sceUsbdInit() == 0, "initialization failed");
    void** list = nullptr;
    const std::int64_t count = sceUsbdGetDeviceList(&list);
    Require(count >= 0 && list != nullptr && list[count] == nullptr, "device list is not a null-terminated array of the reported length");
    for (std::int64_t index = 0; index < count; ++index) {
        std::uint8_t deviceDescriptor[18] = {};
        Require(sceUsbdGetDeviceDescriptor(list[index], deviceDescriptor) == 0 && deviceDescriptor[0] == 18 && deviceDescriptor[1] == 1, "listed device has no device descriptor");
        Require(sceUsbdRefDevice(list[index]) == list[index], "listed device was not referenced");
        sceUsbdUnrefDevice(list[index]);
    }
    sceUsbdFreeDeviceList(list, 1);
    Require(Throws([&] { sceUsbdFreeDeviceList(list, 1); }), "a device list was freed twice");
    EventLocks();
    Require(sceUsbdGetDeviceList(nullptr) == invalidArgument, "null list accepted");
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalidArgument, "null timeout accepted");
    const UsbdTimeval invalid{0, 1000000};
    Require(sceUsbdHandleEventsTimeout(&invalid) == invalidArgument, "out-of-range microseconds accepted");
    const UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");
    int foreign = 0;
    void* handle = &foreign;
    Require(sceUsbdOpen(&foreign, nullptr) == invalidArgument, "null handle output accepted");
    Require(Throws([&] { sceUsbdOpen(nullptr, &handle); }), "opening a null device did not throw");
    Require(Throws([&] { sceUsbdOpen(&foreign, &handle); }), "opening a device that was never listed did not throw");
    sceUsbdClose(nullptr);
    Require(Throws([&] { sceUsbdClose(&foreign); }), "closing a handle that was never opened did not throw");
    Require(sceUsbdRefDevice(nullptr) == nullptr, "null device was referenced");
    sceUsbdUnrefDevice(nullptr);
    Require(Throws([&] { sceUsbdRefDevice(&foreign); }), "referencing a device that was never listed did not throw");
    Require(Throws([&] { sceUsbdUnrefDevice(&foreign); }), "unreferencing a device that was never listed did not throw");
    Require(sceUsbdGetBusNumber(nullptr) == 0 && sceUsbdGetDeviceAddress(nullptr) == 0, "null device reported a bus or address");
    Require(Throws([&] { sceUsbdGetBusNumber(&foreign); }), "bus number of a device that was never listed did not throw");
    std::uint8_t descriptor[18] = {};
    void* config = &foreign;
    Require(sceUsbdGetDeviceDescriptor(nullptr, descriptor) == invalidArgument, "null device descriptor accepted");
    Require(sceUsbdGetDeviceDescriptor(&foreign, nullptr) == invalidArgument, "null descriptor output accepted");
    Require(sceUsbdGetConfigDescriptor(nullptr, 0, &config) == invalidArgument, "null config device accepted");
    Require(sceUsbdGetConfigDescriptor(&foreign, 0, nullptr) == invalidArgument, "null config output accepted");
    Require(Throws([&] { sceUsbdGetConfigDescriptor(&foreign, 0, &config); }), "config of a device that was never listed did not throw");
    Require(Throws([&] { sceUsbdGetActiveConfigDescriptor(nullptr, &config); }), "active config of a null device did not throw");
    Require(sceUsbdClaimInterface(nullptr, 0) == invalidArgument && sceUsbdReleaseInterface(nullptr, 0) == invalidArgument, "null handle interface call accepted");
    Require(sceUsbdSetConfiguration(nullptr, 1) == invalidArgument && sceUsbdResetDevice(nullptr) == invalidArgument, "null handle configuration or reset accepted");
    Require(sceUsbdCheckConnected(nullptr) == invalidArgument, "null handle connection check accepted");
    Require(sceUsbdKernelDriverActive(nullptr, 0) == invalidArgument && sceUsbdAttachKernelDriver(nullptr, 0) == invalidArgument, "null handle kernel driver call accepted");
    Require(sceUsbdControlTransfer(nullptr, 0x80, 6, 0x100, 0, descriptor, 18, 1000) == invalidArgument, "null handle control transfer accepted");
    Require(sceUsbdControlTransfer(&foreign, 0x80, 6, 0x100, 0, nullptr, 18, 1000) == invalidArgument, "control transfer without data accepted");
    Require(Throws([&] { sceUsbdControlTransfer(&foreign, 0x80, 6, 0x100, 0, descriptor, 18, 1000); }), "control transfer on a handle that was never opened did not throw");
    Require(sceUsbdGetStringDescriptor(nullptr, 1, 0x409, descriptor, 18) == invalidArgument, "null handle string descriptor accepted");
    Require(sceUsbdGetStringDescriptor(&foreign, 1, 0x409, descriptor, 0) == invalidArgument, "empty string descriptor buffer accepted");
    descriptor[0] = 0x55;
    Require(sceUsbdGetStringDescriptorAscii(nullptr, 1, descriptor, 18) == invalidArgument, "null handle ASCII string descriptor accepted");
    Require(sceUsbdGetStringDescriptorAscii(&foreign, 1, nullptr, 18) == invalidArgument && sceUsbdGetStringDescriptorAscii(&foreign, 1, descriptor, 0) == invalidArgument, "missing ASCII string buffer accepted");
    Require(descriptor[0] == 0x55, "a rejected ASCII string request wrote the buffer");
    Require(Throws([&] { sceUsbdGetStringDescriptorAscii(&foreign, 1, descriptor, 18); }), "ASCII string request on a handle that was never opened did not throw");
    Require(sceUsbdGetDescriptor(nullptr, 1, 0, descriptor, 18) == invalidArgument, "null handle descriptor request accepted");
    Require(sceUsbdGetDescriptor(&foreign, 1, 0, nullptr, 18) == invalidArgument && sceUsbdGetDescriptor(&foreign, 1, 0, descriptor, 0) == invalidArgument, "missing descriptor buffer accepted");
    Require(Throws([&] { sceUsbdGetDescriptor(&foreign, 1, 0, descriptor, 18); }), "descriptor request on a handle that was never opened did not throw");
    std::int32_t configuration = -1;
    Require(sceUsbdGetConfiguration(nullptr, &configuration) == invalidArgument && sceUsbdGetConfiguration(&foreign, nullptr) == invalidArgument, "null configuration query accepted");
    Require(configuration == -1, "a rejected configuration query wrote the output");
    Require(Throws([&] { sceUsbdGetConfiguration(&foreign, &configuration); }), "configuration of a handle that was never opened did not throw");
    Require(sceUsbdGetConfigDescriptorByValue(nullptr, 1, &config) == invalidArgument && sceUsbdGetConfigDescriptorByValue(&foreign, 1, nullptr) == invalidArgument, "null config by value argument accepted");
    Require(Throws([&] { sceUsbdGetConfigDescriptorByValue(&foreign, 1, &config); }), "config by value of a device that was never listed did not throw");
    Require(Throws([&] { sceUsbdClaimInterface(&foreign, 0); }), "claiming on a handle that was never opened did not throw");
    auto* transfer = static_cast<std::uint8_t*>(sceUsbdAllocTransfer(2));
    Require(transfer != nullptr, "transfer allocation failed");
    Require(*reinterpret_cast<std::int32_t*>(transfer + 56) == 2, "iso packet count not stored");
    Require(sceUsbdAllocTransfer(-1) == nullptr, "negative iso packet count accepted");
    std::uint8_t buffer[8] = {};
    int user = 0;
    sceUsbdFillInterruptTransfer(transfer, nullptr, 0x81, buffer, 8, reinterpret_cast<void*>(&Require), &user, 250);
    Require(transfer[9] == 0x81 && transfer[10] == 3, "endpoint or interrupt type not filled");
    Require(*reinterpret_cast<std::uint32_t*>(transfer + 12) == 250 && *reinterpret_cast<std::int32_t*>(transfer + 20) == 8, "timeout or length not filled");
    Require(*reinterpret_cast<void**>(transfer + 40) == &user && *reinterpret_cast<std::uint8_t**>(transfer + 48) == buffer, "user data or buffer not filled");
    Require(sceUsbdCancelTransfer(transfer) == static_cast<std::int32_t>(0x80240005), "cancelling an unsubmitted transfer did not report not found");
    Require(sceUsbdSubmitTransfer(nullptr) == invalidArgument, "null transfer submitted");
    Require(sceUsbdSubmitTransfer(transfer) == invalidArgument, "transfer without a handle submitted");
    sceUsbdFillInterruptTransfer(transfer, &foreign, 0x81, buffer, 8, reinterpret_cast<void*>(&Require), &user, 250);
    Require(Throws([&] { sceUsbdSubmitTransfer(transfer); }), "submitting on a handle that was never opened did not throw");
    sceUsbdFreeTransfer(transfer);
    Require(Throws([&] { sceUsbdClearHalt(nullptr, 0x81); }), "clearing a halt on a null handle did not throw");
    Require(Throws([&] { sceUsbdClearHalt(&foreign, 0x81); }), "clearing a halt on a handle that was never opened did not throw");
    Require(sceUsbdDetachKernelDriver(nullptr, 0) == invalidArgument, "null handle kernel driver detach accepted");
    Require(Throws([&] { sceUsbdDetachKernelDriver(&foreign, 0); }), "kernel driver detach on a handle that was never opened did not throw");
    Require(sceUsbdGetDevice(nullptr) == nullptr, "a null handle has a device");
    Require(Throws([&] { sceUsbdGetDevice(&foreign); }), "the device of a handle that was never opened did not throw");
    Require(sceUsbdGetDeviceSpeed(nullptr) == 0, "a null device has a speed");
    Require(Throws([&] { sceUsbdGetDeviceSpeed(&foreign); }), "the speed of a device that was never listed did not throw");
    Require(sceUsbdGetMaxPacketSize(nullptr, 0x81) == noDevice, "a null device has a max packet size");
    Require(Throws([&] { sceUsbdGetMaxPacketSize(&foreign, 0x81); }), "the max packet size of a device that was never listed did not throw");
    Require(Throws([&] { sceUsbdGetMaxIsoPacketSize(nullptr, 0x81); }), "the max iso packet size of a null device did not throw");
    Require(Throws([&] { sceUsbdGetMaxIsoPacketSize(&foreign, 0x81); }), "the max iso packet size of a device that was never listed did not throw");
    Require(sceUsbdSetInterfaceAltSetting(nullptr, 0, 0) == invalidArgument, "null handle alternate setting accepted");
    Require(Throws([&] { sceUsbdSetInterfaceAltSetting(&foreign, 0, 0); }), "an alternate setting on a handle that was never opened did not throw");
    Require(sceUsbdOpenDeviceWithVidPid(0, 0) == nullptr, "a device with vendor and product 0 was opened");
    sceUsbdFreeTransfer(nullptr);
    Require(sceUsbdEventHandlingOk() == 0, "a thread without the event lock may handle events");
    sceUsbdFreeConfigDescriptor(nullptr);
    Require(Throws([] { sceUsbdFillControlSetup(nullptr, 0x80, 6, 0x0100, 0, 18); }), "null control setup buffer accepted");
    std::uint8_t setupBuffer[8] = {};
    sceUsbdFillControlSetup(setupBuffer, 0x80, 6, 0x0100, 0x0409, 18);
    Require(setupBuffer[0] == 0x80 && setupBuffer[1] == 6, "request type or request not filled");
    Require(setupBuffer[2] == 0x00 && setupBuffer[3] == 0x01, "value not filled little-endian");
    Require(setupBuffer[4] == 0x09 && setupBuffer[5] == 0x04, "index not filled little-endian");
    Require(setupBuffer[6] == 18 && setupBuffer[7] == 0, "length not filled little-endian");
    if (argc == 2 && std::strcmp(argv[1], "device") == 0) Device();
    sceUsbdExit();
    Require(sceUsbdOpenDeviceWithVidPid(0, 0) == nullptr, "a device was opened after the library exited");
    std::puts("USBD tests passed");
    return 0;
}
