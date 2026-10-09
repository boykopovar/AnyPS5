#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct UsbdTransfer;

using UsbdTransferCallback = void (APS5_VABI *)(UsbdTransfer*);

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
    std::int32_t isoPacketCount;
};

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
int APS5_VABI sceUsbdOpen();
UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPacketCount);
void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, void* handle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdSubmitTransfer(UsbdTransfer* transfer);
std::int32_t APS5_VABI sceUsbdCancelTransfer(UsbdTransfer* transfer);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdClose(void* handle);
void* APS5_VABI sceUsbdRefDevice(void* device);
void APS5_VABI sceUsbdUnrefDevice(void* device);
std::uint8_t APS5_VABI sceUsbdGetBusNumber(void* device);
std::uint8_t APS5_VABI sceUsbdGetDeviceAddress(void* device);
std::int32_t APS5_VABI sceUsbdGetDeviceDescriptor(void* device, void* descriptor);
std::int32_t APS5_VABI sceUsbdGetConfigDescriptor(void* device, std::uint8_t configIndex, void** config);
void APS5_VABI sceUsbdFreeConfigDescriptor(void* config);
std::int32_t APS5_VABI sceUsbdClaimInterface(void* handle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdReleaseInterface(void* handle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdSetConfiguration(void* handle, std::int32_t configuration);
std::int32_t APS5_VABI sceUsbdResetDevice(void* handle);
std::int32_t APS5_VABI sceUsbdCheckConnected(void* handle);
std::int32_t APS5_VABI sceUsbdKernelDriverActive(void* handle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdAttachKernelDriver(void* handle, std::int32_t interfaceNumber);
std::int32_t APS5_VABI sceUsbdControlTransfer(void* handle, std::uint8_t requestType, std::uint8_t request, std::uint16_t value, std::uint16_t index, std::uint8_t* data, std::uint16_t length, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdGetStringDescriptor(void* handle, std::uint8_t descriptorIndex, std::uint16_t languageId, std::uint8_t* data, std::int32_t length);
}

namespace {

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t notFound = static_cast<std::int32_t>(0x80240005);

std::size_t releases = 0;
void* lastRelease = nullptr;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

template<typename TCall>
void RequireThrows(TCall call, const char* message) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, message);
}

void* APS5_VABI allocate(std::size_t bytes) {
    return std::malloc(bytes);
}

void APS5_VABI release(void* pointer) {
    ++releases;
    lastRelease = pointer;
    std::free(pointer);
}

void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI posixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void APS5_VABI completed(UsbdTransfer*) {
    std::abort();
}

void TestTransfers() {
    UsbdTransfer* transfer = sceUsbdAllocTransfer(0);
    Require(transfer != nullptr, "transfer allocation failed");
    Require(transfer->deviceHandle == nullptr && transfer->flags == 0 && transfer->buffer == nullptr && transfer->callback == nullptr && transfer->isoPacketCount == 0, "allocated transfer is not zeroed");
    Require(sceUsbdSubmitTransfer(nullptr) == invalidArgument, "null transfer submitted");
    Require(sceUsbdSubmitTransfer(transfer) == invalidArgument, "transfer without a device handle submitted");
    Require(sceUsbdCancelTransfer(nullptr) == invalidArgument, "null transfer cancelled");
    Require(sceUsbdCancelTransfer(transfer) == notFound, "transfer without a device handle was found in flight");

    std::array<std::uint8_t, 8> data{};
    int userData = 0;
    transfer->flags = 1;
    transfer->status = 3;
    transfer->actualLength = 5;
    sceUsbdFillInterruptTransfer(transfer, nullptr, 0x81, data.data(), static_cast<std::int32_t>(data.size()), completed, &userData, 250);
    Require(transfer->deviceHandle == nullptr && transfer->endpoint == 0x81 && transfer->type == 3 && transfer->timeout == 250, "interrupt transfer header not filled");
    Require(transfer->buffer == data.data() && transfer->length == 8 && transfer->callback == completed && transfer->userData == &userData, "interrupt transfer payload not filled");
    Require(transfer->flags == 1 && transfer->status == 3 && transfer->actualLength == 5 && transfer->isoPacketCount == 0, "fill changed fields it does not own");
    Require(sceUsbdSubmitTransfer(transfer) == invalidArgument, "filled transfer without a device handle submitted");

    int fakeHandle = 0;
    sceUsbdFillInterruptTransfer(transfer, &fakeHandle, 0x81, data.data(), static_cast<std::int32_t>(data.size()), completed, &userData, 250);
    Require(transfer->deviceHandle == &fakeHandle, "device handle not stored");
    RequireThrows([&] { sceUsbdSubmitTransfer(transfer); }, "transfer with an unopened device handle submitted");
    RequireThrows([&] { sceUsbdCancelTransfer(transfer); }, "transfer with an unopened device handle cancelled");
    RequireThrows([] { sceUsbdFillInterruptTransfer(nullptr, nullptr, 0, nullptr, 0, nullptr, nullptr, 0); }, "null transfer filled");
    const std::size_t releasesBefore = releases;
    sceUsbdFreeTransfer(transfer);
    Require(releases == releasesBefore, "buffer freed without the free-buffer flag");

    UsbdTransfer* iso = sceUsbdAllocTransfer(4);
    Require(iso != nullptr && iso->isoPacketCount == 4, "iso transfer allocation failed");
    auto* owned = static_cast<std::uint8_t*>(ApplicationHeapAllocate_nid_no_patch(16));
    iso->flags = 2;
    iso->buffer = owned;
    sceUsbdFreeTransfer(iso);
    Require(releases == releasesBefore + 1 && lastRelease == owned, "free-buffer flag did not free the buffer through libc");

    sceUsbdFreeTransfer(nullptr);
    UsbdTransfer foreign{};
    RequireThrows([&] { sceUsbdFreeTransfer(&foreign); }, "foreign transfer freed");
    RequireThrows([] { sceUsbdAllocTransfer(-1); }, "negative iso packet count accepted");
}

void TestDevices() {
    int fake = 0;
    Require(sceUsbdRefDevice(nullptr) == nullptr, "null device referenced");
    sceUsbdUnrefDevice(nullptr);
    Require(sceUsbdGetBusNumber(nullptr) == 0, "null device has a bus number");
    Require(sceUsbdGetDeviceAddress(nullptr) == 0, "null device has an address");
    std::array<std::uint8_t, 18> descriptor{};
    Require(sceUsbdGetDeviceDescriptor(nullptr, descriptor.data()) == invalidArgument, "null device described");
    Require(sceUsbdGetDeviceDescriptor(&fake, nullptr) == invalidArgument, "null device descriptor accepted");
    void* config = &fake;
    Require(sceUsbdGetConfigDescriptor(nullptr, 0, &config) == invalidArgument && config == &fake, "null device configuration returned");
    Require(sceUsbdGetConfigDescriptor(&fake, 0, nullptr) == invalidArgument, "null config output accepted");
    sceUsbdFreeConfigDescriptor(nullptr);
    RequireThrows([&] { sceUsbdRefDevice(&fake); }, "unlisted device referenced");
    RequireThrows([&] { sceUsbdUnrefDevice(&fake); }, "unlisted device unreferenced");
    RequireThrows([&] { sceUsbdGetBusNumber(&fake); }, "unlisted device has a bus number");
    RequireThrows([&] { sceUsbdGetDeviceAddress(&fake); }, "unlisted device has an address");
    RequireThrows([&] { sceUsbdGetDeviceDescriptor(&fake, descriptor.data()); }, "unlisted device described");
    RequireThrows([&] { sceUsbdGetConfigDescriptor(&fake, 0, &config); }, "unlisted device configuration returned");
    RequireThrows([&] { sceUsbdFreeConfigDescriptor(&fake); }, "foreign config descriptor freed");
}

void TestHandles() {
    int fake = 0;
    std::array<std::uint8_t, 4> data{};
    sceUsbdClose(nullptr);
    Require(sceUsbdClaimInterface(nullptr, 0) == invalidArgument, "null handle claimed an interface");
    Require(sceUsbdReleaseInterface(nullptr, 0) == invalidArgument, "null handle released an interface");
    Require(sceUsbdSetConfiguration(nullptr, 1) == invalidArgument, "null handle configured");
    Require(sceUsbdResetDevice(nullptr) == invalidArgument, "null handle reset");
    Require(sceUsbdCheckConnected(nullptr) == invalidArgument, "null handle connected");
    Require(sceUsbdKernelDriverActive(nullptr, 0) == invalidArgument, "null handle has a kernel driver");
    Require(sceUsbdAttachKernelDriver(nullptr, 0) == invalidArgument, "null handle attached a kernel driver");
    Require(sceUsbdControlTransfer(nullptr, 0x80, 6, 0x100, 0, data.data(), 4, 100) == invalidArgument, "null handle transferred");
    data[0] = 0x55;
    Require(sceUsbdGetStringDescriptor(nullptr, 1, 0x409, data.data(), 4) == invalidArgument, "null handle described a string");
    Require(sceUsbdGetStringDescriptor(&fake, 1, 0x409, nullptr, 4) == invalidArgument, "null string buffer accepted");
    Require(sceUsbdGetStringDescriptor(&fake, 1, 0x409, data.data(), 0) == invalidArgument, "empty string buffer accepted");
    Require(data[0] == 0x55, "rejected string descriptor wrote data");
    RequireThrows([&] { sceUsbdClose(&fake); }, "unopened handle closed");
    RequireThrows([&] { sceUsbdClaimInterface(&fake, 0); }, "unopened handle claimed an interface");
    RequireThrows([&] { sceUsbdReleaseInterface(&fake, 0); }, "unopened handle released an interface");
    RequireThrows([&] { sceUsbdSetConfiguration(&fake, 1); }, "unopened handle configured");
    RequireThrows([&] { sceUsbdResetDevice(&fake); }, "unopened handle reset");
    RequireThrows([&] { sceUsbdCheckConnected(&fake); }, "unopened handle connected");
    RequireThrows([&] { sceUsbdKernelDriverActive(&fake, 0); }, "unopened handle has a kernel driver");
    RequireThrows([&] { sceUsbdAttachKernelDriver(&fake, 0); }, "unopened handle attached a kernel driver");
    RequireThrows([&] { sceUsbdControlTransfer(&fake, 0x80, 6, 0x100, 0, data.data(), 4, 100); }, "unopened handle transferred");
    RequireThrows([&] { sceUsbdGetStringDescriptor(&fake, 1, 0x409, data.data(), 4); }, "unopened handle described a string");
}

}

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&allocate), reinterpret_cast<void*>(&release), reinterpret_cast<void*>(&allocateZeroed), reinterpret_cast<void*>(&reallocate), reinterpret_cast<void*>(&align), reinterpret_cast<void*>(&realign), reinterpret_cast<void*>(&posixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    Require(sceUsbdInit() == 0, "initialization failed");
    void** list = nullptr;
    Require(sceUsbdGetDeviceList(&list) == 0, "a device was listed");
    Require(list != nullptr && list[0] == nullptr, "device list is not an empty null-terminated array");
    sceUsbdFreeDeviceList(list, 1);
    Require(sceUsbdGetDeviceList(nullptr) == invalidArgument, "null list accepted");
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalidArgument, "null timeout accepted");
    const UsbdTimeval invalid{0, 1000000};
    Require(sceUsbdHandleEventsTimeout(&invalid) == invalidArgument, "out-of-range microseconds accepted");
    const UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");
    Require(sceUsbdEventHandlingOk() == 0, "event handling allowed on a thread that does not hold the event lock");
    RequireThrows([] { sceUsbdOpen(); }, "opening a device did not throw");
    TestTransfers();
    TestDevices();
    TestHandles();
    sceUsbdExit();
    std::puts("USBD tests passed");
    return 0;
}
