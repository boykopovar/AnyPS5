#include "prx/libSceUsbStorage/UsbStorage.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

extern "C" {
int APS5_VABI sceUsbStorageGetMountPointOfShellCore(unsigned int device, char* mountPoint);
int APS5_VABI sceUsbStorageClearFakeWriteProtection();
int APS5_VABI sceUsbStorageSetFakeMapLockForDebug();
int APS5_VABI sceUsbStorageSetFakeWriteProtection();
}

namespace {

int g_attr = 0;

void APS5_VABI CallbackStub(const UsbStorageEvent*, void*) {}

UsbStorageCallback Callback() {
    return &CallbackStub;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBSTORAGE: %s\n", message);
        std::abort();
    }
}

void TestInitializeAndTerminate() {
    Require(sceUsbStorageInit(nullptr) == 0, "initialize with a null attribute");
    Require(sceUsbStorageInit(&g_attr) == 0, "initialize with an attribute");
    Require(sceUsbStorageTerm() == 0, "terminate");
    Require(sceUsbStorageInit(nullptr) == 0, "initialize after terminate");
    Require(sceUsbStorageTerm() == 0, "terminate again");
}

void TestRegisterCallbacks() {
    bool threw = false;
    try {
        sceUsbStorageRegisterCallback(1, Callback(), nullptr);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRegisterCallback not implemented";
    }
    Require(threw, "register callback of event type 1 did not throw its marker");
    threw = false;
    try {
        sceUsbStorageRegisterCallback(2, Callback(), nullptr);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRegisterCallback not implemented";
    }
    Require(threw, "register callback of event type 2 did not throw its marker");
    Require(sceUsbStorageRegisterCallback(1, nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null callback");
    Require(sceUsbStorageRegisterCallback(0, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of event type 0");
    Require(sceUsbStorageRegisterCallback(3, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of event type 3");
    Require(sceUsbStorageRegisterCallback(-1, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of a negative event type");
    threw = false;
    try {
        sceUsbStorageUnregisterCallback(1, Callback());
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageUnregisterCallback not implemented";
    }
    Require(threw, "unregister callback of event type 1 did not throw its marker");
    threw = false;
    try {
        sceUsbStorageUnregisterCallback(2, Callback());
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageUnregisterCallback not implemented";
    }
    Require(threw, "unregister callback of event type 2 did not throw its marker");
    Require(sceUsbStorageUnregisterCallback(1, nullptr) == UsbStorage::InvalidArgument, "unregister a null callback");
    Require(sceUsbStorageUnregisterCallback(0, Callback()) == UsbStorage::InvalidArgument, "unregister callback of event type 0");
}

void TestMapCallbacks() {
    bool threw = false;
    try {
        sceUsbStorageRegisterCallbackForMapAvailable(0, "/data", Callback(), nullptr);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRegisterCallbackForMapAvailable not implemented";
    }
    Require(threw, "register map-available callback did not throw its marker");
    Require(sceUsbStorageRegisterCallbackForMapAvailable(0, nullptr, Callback(), nullptr) == UsbStorage::InvalidArgument, "register map-available callback with a null directory");
    Require(sceUsbStorageRegisterCallbackForMapAvailable(0, "/data", nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null map-available callback");
    threw = false;
    try {
        sceUsbStorageUnregisterCallbackForMapAvailable(0, "/data", Callback());
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageUnregisterCallbackForMapAvailable not implemented";
    }
    Require(threw, "unregister map-available callback did not throw its marker");
    Require(sceUsbStorageUnregisterCallbackForMapAvailable(0, nullptr, Callback()) == UsbStorage::InvalidArgument, "unregister map-available callback with a null directory");
    Require(sceUsbStorageUnregisterCallbackForMapAvailable(0, "/data", nullptr) == UsbStorage::InvalidArgument, "unregister a null map-available callback");
    threw = false;
    try {
        sceUsbStorageRegisterCallbackForMapUnavailable(0, "/data", Callback(), nullptr);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRegisterCallbackForMapUnavailable not implemented";
    }
    Require(threw, "register map-unavailable callback did not throw its marker");
    Require(sceUsbStorageRegisterCallbackForMapUnavailable(0, nullptr, Callback(), nullptr) == UsbStorage::InvalidArgument, "register map-unavailable callback with a null directory");
    Require(sceUsbStorageRegisterCallbackForMapUnavailable(0, "/data", nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null map-unavailable callback");
    threw = false;
    try {
        sceUsbStorageUnregisterCallbackForMapUnavailable(0, "/data", Callback());
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageUnregisterCallbackForMapUnavailable not implemented";
    }
    Require(threw, "unregister map-unavailable callback did not throw its marker");
    Require(sceUsbStorageUnregisterCallbackForMapUnavailable(0, nullptr, Callback()) == UsbStorage::InvalidArgument, "unregister map-unavailable callback with a null directory");
    Require(sceUsbStorageUnregisterCallbackForMapUnavailable(0, "/data", nullptr) == UsbStorage::InvalidArgument, "unregister a null map-unavailable callback");
}

void TestNoDevice() {
    std::uint32_t devices[4]{0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0, "get the device list");
    Require(count == 0, "no usb storage device is attached");
    Require(devices[0] == 0x5a5a5a5a && devices[3] == 0x5a5a5a5a, "the device list was written");
    Require(sceUsbStorageGetDeviceList(nullptr, &count) == UsbStorage::InvalidArgument, "get the device list without a device array");
    Require(sceUsbStorageGetDeviceList(devices, nullptr) == UsbStorage::InvalidArgument, "get the device list without a count");

    std::uint8_t info[16];
    std::memset(info, 0x5a, sizeof(info));
    auto* const infoRecord = reinterpret_cast<UsbStorageDeviceInfo*>(info);
    Require(sceUsbStorageGetDeviceInfo(0, infoRecord) == UsbStorage::DeviceNotFound, "get device info of device 0");
    Require(sceUsbStorageGetDeviceInfo(1, infoRecord) == UsbStorage::DeviceNotFound, "get device info of device 1");
    Require(info[0] == 0x5a && info[15] == 0x5a, "device info was written");
    Require(sceUsbStorageGetDeviceInfo(0, nullptr) == UsbStorage::InvalidArgument, "get device info without an info buffer");

    std::uint8_t exists = 1;
    Require(sceUsbStorageIsExist(0, "/mnt/usb0", &exists) == true && exists == 0, "device 0 does not exist");
    exists = 1;
    Require(sceUsbStorageIsExist(1, nullptr, &exists) == true && exists == 0, "device 1 does not exist");
    exists = 1;
    Require(sceUsbStorageIsExist(0xffffffffu, "/mnt/usb0", &exists) == true && exists == 0, "no device id exists");
}

void TestMapRequests() {
    char mountPoint[16];
    std::uint64_t mountId = 0x5a5a5a5a5a5a5a5aull;
    std::int32_t status = 0x5a5a5a5a;
    std::memset(mountPoint, 0x5a, sizeof(mountPoint));
    Require(sceUsbStorageRequestMap(0, "/data", 0, 0, mountPoint, &mountId) == UsbStorage::DeviceNotFound, "request a map of device 0");
    Require(sceUsbStorageRequestMap(1, nullptr, 0, 0, nullptr, nullptr) == UsbStorage::DeviceNotFound, "request a map of device 1");
    Require(mountId == 0x5a5a5a5a5a5a5a5aull && mountPoint[0] == static_cast<char>(0x5a), "the map request wrote its outputs");
    std::memset(mountPoint, 0x5a, sizeof(mountPoint));
    Require(sceUsbStorageRequestMapWSB(0, "/data", 0, 0, mountPoint, &status) == UsbStorage::DeviceNotFound, "request a WSB map of device 0");
    Require(sceUsbStorageRequestMapWSB(0, nullptr, 0, 0, nullptr, nullptr) == UsbStorage::DeviceNotFound, "request a WSB map without a subpath");
    Require(mountPoint[0] == static_cast<char>(0x5a) && status == 0x5a5a5a5a, "the WSB map request wrote its outputs");
    Require(sceUsbStorageRequestUnmap(0, nullptr) == UsbStorage::InvalidArgument, "unmap a null directory");
    bool threw = false;
    try {
        sceUsbStorageRequestUnmap(0, "/mnt/usb0");
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRequestUnmap not implemented";
    }
    Require(threw, "unmap a directory did not throw its marker");
    threw = false;
    try {
        sceUsbStorageRequestUnmap(7, "/mnt/usb7");
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageRequestUnmap not implemented";
    }
    Require(threw, "unmap a directory of another device did not throw its marker");
}

void TestGetdents() {
    std::int32_t fd = -1;
    std::uint64_t bufferSize = 0x5a5a5a5a5a5a5a5aull;
    Require(sceUsbStorageGetdentsOpen(0, &fd, &bufferSize) == UsbStorage::DeviceNotFound, "open a getdents scan of device 0");
    Require(sceUsbStorageGetdentsOpen(3, &fd, &bufferSize) == UsbStorage::DeviceNotFound, "open a getdents scan of device 3");
    Require(fd == -1 && bufferSize == 0x5a5a5a5a5a5a5a5aull, "the getdents open wrote its outputs");
    Require(sceUsbStorageGetdentsOpen(0, nullptr, &bufferSize) == UsbStorage::InvalidArgument, "open a getdents scan without a descriptor");
    Require(sceUsbStorageGetdentsOpen(0, &fd, nullptr) == UsbStorage::InvalidArgument, "open a getdents scan without a buffer size");

    std::uint8_t buffer[64];
    std::uint64_t bytesRead = 0x5a5a5a5a5a5a5a5aull;
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(sceUsbStorageGetdentsRead(1, buffer, sizeof(buffer), &bytesRead) == UsbStorage::NotSupported, "read a getdents scan that was never opened");
    Require(bytesRead == 0, "the getdents read did not answer zero bytes");
    Require(buffer[0] == 0x5a, "the getdents read wrote the buffer");
    Require(sceUsbStorageGetdentsRead(1, nullptr, sizeof(buffer), &bytesRead) == UsbStorage::InvalidArgument, "read a getdents scan without a buffer");
    Require(sceUsbStorageGetdentsRead(1, buffer, sizeof(buffer), nullptr) == UsbStorage::InvalidArgument, "read a getdents scan without a byte count");

    bool threw = false;
    try {
        sceUsbStorageGetdentsClose(1);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageGetdentsClose not implemented";
    }
    Require(threw, "close a getdents scan that was never opened did not throw its marker");
    threw = false;
    try {
        sceUsbStorageGetdentsClose(0);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageGetdentsClose not implemented";
    }
    Require(threw, "close descriptor 0 did not throw its marker");
}

void TestThrowingExports() {
    char mountPoint[8]{};
    bool threw = false;
    try {
        sceUsbStorageGetMountPointOfShellCore(0, mountPoint);
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageGetMountPointOfShellCore not implemented";
    }
    Require(threw, "get the mount point of shell core did not throw its marker");
    threw = false;
    try {
        sceUsbStorageClearFakeWriteProtection();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageClearFakeWriteProtection not implemented";
    }
    Require(threw, "clear fake write protection did not throw its marker");
    threw = false;
    try {
        sceUsbStorageSetFakeMapLockForDebug();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageSetFakeMapLockForDebug not implemented";
    }
    Require(threw, "set fake map lock for debug did not throw its marker");
    threw = false;
    try {
        sceUsbStorageSetFakeWriteProtection();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()) == "sceUsbStorageSetFakeWriteProtection not implemented";
    }
    Require(threw, "set fake write protection did not throw its marker");
}

}

int main() {
    TestInitializeAndTerminate();
    TestRegisterCallbacks();
    TestMapCallbacks();
    TestNoDevice();
    TestMapRequests();
    TestGetdents();
    TestThrowingExports();
    return 0;
}
