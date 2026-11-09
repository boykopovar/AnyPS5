#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceUsbStorage/UsbStorage.hpp"
#include "prx/libSceUsbStorage/include/HostVolumes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {

void APS5_VABI CallbackStub(const UsbStorageEvent*, void*) {}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "SYSTEMSERVICE USBSTORAGE: %s\n", message);
        std::abort();
    }
}

void WriteFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
    file.close();
    Require(static_cast<bool>(file), "write a fake host file");
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

void SetPassthrough(bool enabled) {
#ifdef _WIN32
    Require(_putenv_s("APS5_USB_STORAGE_PASSTHROUGH", enabled ? "1" : "") == 0, "set the passthrough switch");
#else
    Require((enabled ? ::setenv("APS5_USB_STORAGE_PASSTHROUGH", "1", 1) : ::unsetenv("APS5_USB_STORAGE_PASSTHROUGH")) == 0, "set the passthrough switch");
#endif
}

void TestNoDevice() {
    Require(sceSystemServiceUsbStorageInit(nullptr) == 0, "initialize through the alias");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceSystemServiceUsbStorageGetDeviceList(devices, &count) == 0 && count == 0, "the alias lists no device");
    Require(sceSystemServiceUsbStorageGetDeviceList(nullptr, &count) == UsbStorage::InvalidArgument, "the alias refuses a null device list");
    UsbStorageDeviceInfo info{};
    Require(sceSystemServiceUsbStorageGetDeviceInfo(0, &info) == UsbStorage::DeviceNotFound, "the alias finds no device info");
    std::uint8_t exists = 1;
    Require(sceSystemServiceUsbStorageIsExist(0, "/mnt/usb0", &exists) == true && exists == 0, "the alias reports no device");
    char mountPoint[UsbStorage::MountPointSize];
    std::uint64_t mountId = 0;
    Require(sceSystemServiceUsbStorageRequestMap(0, "/", 0, 0, mountPoint, &mountId) == UsbStorage::DeviceNotFound, "the alias maps no device");
    Require(sceSystemServiceUsbStorageRequestUnmap(0, nullptr) == UsbStorage::InvalidArgument, "the alias refuses a null unmap");
    std::int32_t fd = -1;
    std::uint64_t bufferSize = 0;
    Require(sceSystemServiceUsbStorageGetdentsOpen(0, &fd, &bufferSize) == UsbStorage::DeviceNotFound, "the alias opens no scan");
    std::uint8_t buffer[64];
    std::uint64_t bytesRead = 1;
    Require(sceSystemServiceUsbStorageGetdentsRead(1, buffer, sizeof(buffer), &bytesRead) == UsbStorage::NotSupported && bytesRead == 0,
            "the alias reads no scan");
    Require(sceSystemServiceUsbStorageRegisterCallback(1, &CallbackStub, nullptr) == 0, "register a callback through the alias");
    Require(sceSystemServiceUsbStorageRegisterCallback(3, &CallbackStub, nullptr) == UsbStorage::InvalidArgument, "the alias refuses event type 3");
    Require(sceUsbStorageUnregisterCallback(1, &CallbackStub) == 0, "the alias registered with the USB storage service");
    Require(sceSystemServiceUsbStorageRegisterCallback(2, &CallbackStub, nullptr) == 0, "register an unmount callback through the alias");
    Require(sceSystemServiceUsbStorageUnregisterCallback(2, &CallbackStub) == 0, "unregister a callback through the alias");
    Require(sceSystemServiceUsbStorageUnregisterCallback(0, &CallbackStub) == UsbStorage::InvalidArgument, "the alias refuses an unregistration of event type 0");
    Require(sceSystemServiceUsbStorageRegisterCallbackForMapAvailable(0, nullptr, &CallbackStub, nullptr) == UsbStorage::InvalidArgument,
            "the map-available alias refuses a null directory");
    Require(sceSystemServiceUsbStorageUnregisterCallbackForMapAvailable(0, "/", nullptr) == UsbStorage::InvalidArgument,
            "the map-available alias refuses a null callback");
    Require(sceSystemServiceUsbStorageRegisterCallbackForMapUnavailable(0, nullptr, &CallbackStub, nullptr) == UsbStorage::InvalidArgument,
            "the map-unavailable alias refuses a null directory");
    Require(sceSystemServiceUsbStorageUnregisterCallbackForMapUnavailable(0, "/", nullptr) == UsbStorage::InvalidArgument,
            "the map-unavailable alias refuses a null callback");
    Require(sceSystemServiceUsbStorageTerm() == 0, "terminate through the alias");
}

void TestDevice() {
    Require(sceSystemServiceUsbStorageInit(nullptr) == 0, "initialize through the alias with a device");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 1 && devices[0] == 0, "the alias initialized the USB storage service");
    UsbStorageDeviceInfo info{};
    Require(sceSystemServiceUsbStorageGetDeviceInfo(0, &info) == 0 && info.vendorId == 0x0781 && info.productId == 0x5581, "the alias reads the device info");
    std::uint8_t exists = 0;
    Require(sceSystemServiceUsbStorageIsExist(0, "/", &exists) == true && exists == 1, "the alias reports the root of the device");
    char mountPoint[UsbStorage::MountPointSize]{};
    std::uint64_t mountId = 1;
    Require(sceSystemServiceUsbStorageRequestMap(0, "/", 0, 0, mountPoint, &mountId) == 0, "map the device through the alias");
    Require(std::string_view(mountPoint) == "/mnt/usb0" && mountId == 0, "the alias returns the mount point");
    Require(sceSystemServiceUsbStorageRequestUnmap(0, "/mnt/usb0") == 0, "unmap the device through the alias");
    std::int32_t fd = -1;
    std::uint64_t bufferSize = 0;
    Require(sceSystemServiceUsbStorageGetdentsOpen(0, &fd, &bufferSize) == 0 && bufferSize == 0x8000, "open a scan through the alias");
    std::uint8_t buffer[0x100]{};
    std::uint64_t bytesRead = 0;
    Require(sceSystemServiceUsbStorageGetdentsRead(fd, buffer, sizeof(buffer), &bytesRead) == 0 && bytesRead == 24, "read the scan through the alias");
    Require(std::string_view(reinterpret_cast<const char*>(buffer + 8)) == "song.txt", "the alias scan lists the device root");
    Require(sceSystemServiceUsbStorageGetdentsClose(fd) == 0, "close the scan through the alias");
    Require(sceSystemServiceUsbStorageTerm() == 0, "terminate through the alias with a device");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_system_service_usb_storage_" + std::to_string(std::random_device{}()));
    WriteFile(root / "drive" / "song.txt", "song");
    static const UsbStorageTestVolume none{};
    SetUsbStorageHostVolumesForTest_nid_no_patch(&none, 0);
    TestNoDevice();
    const auto drive = Utf8(root / "drive");
    const UsbStorageTestVolume volume{"drive", drive.c_str(), 61530439680ull, 0x0781, 0x5581, 0x0100, "SanDisk", "Ultra Fit", "4C530001", false, false};
    SetPassthrough(true);
    SetUsbStorageHostVolumesForTest_nid_no_patch(&volume, 1);
    TestDevice();
    SetUsbStorageHostVolumesForTest_nid_no_patch(nullptr, 0);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return 0;
}
