#include "prx/libSceUsbStorage/UsbStorage.hpp"
#include "prx/libSceUsbStorage/include/HostVolumes.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {

int g_attr = 0;
int g_mountTag = 0;
int g_unmountTag = 0;

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

template <typename TCall>
bool ThrowsWith(TCall call, std::string_view message) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string_view(error.what()) == message;
    }
    return false;
}

struct RecordedEvent {
    UsbStorageEvent event;
    void* userData;
};

struct Recorder {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<RecordedEvent> events;
};

Recorder& Events() {
    static Recorder recorder;
    return recorder;
}

void APS5_VABI Record(const UsbStorageEvent* event, void* userData) {
    auto& recorder = Events();
    std::lock_guard lock(recorder.mutex);
    recorder.events.push_back({*event, userData});
    recorder.changed.notify_all();
}

RecordedEvent NextEvent(std::size_t index) {
    auto& recorder = Events();
    std::unique_lock lock(recorder.mutex);
    const bool arrived = recorder.changed.wait_for(lock, std::chrono::seconds(10), [&] { return recorder.events.size() > index; });
    Require(arrived, "no USB storage event arrived within 10 seconds");
    return recorder.events[index];
}

std::size_t RecordedCount() {
    auto& recorder = Events();
    std::lock_guard lock(recorder.mutex);
    return recorder.events.size();
}

std::atomic<int> g_selfRemovalCalls{0};
std::atomic<std::int32_t> g_selfRemovalResult{-1};

void APS5_VABI RemoveSelf(const UsbStorageEvent*, void*) {
    ++g_selfRemovalCalls;
    g_selfRemovalResult = sceUsbStorageUnregisterCallback(UsbStorage::MountEvent, &RemoveSelf);
}

struct Blocker {
    std::mutex mutex;
    std::condition_variable changed;
    int calls = 0;
    bool entered = false;
    bool released = false;
};

Blocker& Blocked() {
    static Blocker blocker;
    return blocker;
}

void APS5_VABI BlockInside(const UsbStorageEvent*, void*) {
    auto& blocker = Blocked();
    std::unique_lock lock(blocker.mutex);
    ++blocker.calls;
    blocker.entered = true;
    blocker.changed.notify_all();
    blocker.changed.wait_for(lock, std::chrono::seconds(10), [&] { return blocker.released; });
}

struct FakeHost {
    std::filesystem::path root;
    std::filesystem::path mountInfo;
    std::filesystem::path sys;
    std::filesystem::path firstMount;
    std::filesystem::path secondMount;
    std::filesystem::path movedMount;
};

void WriteFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
    file.close();
    Require(static_cast<bool>(file), "write a fake host file");
}

void WriteMounts(const FakeHost& host, const std::string& lines) {
    auto staged = host.mountInfo;
    staged += ".new";
    WriteFile(staged, lines);
    std::error_code error;
    for (int attempt = 0; attempt < 500; ++attempt) {
        std::filesystem::rename(staged, host.mountInfo, error);
        if (!error) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(false, "replace the fake mountinfo");
}

std::string Escaped(const std::filesystem::path& path) {
    std::string escaped;
    for (const char character : path.string()) {
        if (character == ' ') escaped += "\\040";
        else if (character == '\\') escaped += "\\134";
        else escaped += character;
    }
    return escaped;
}

std::string MountLine(std::string_view device, std::string_view root, const std::filesystem::path& mount, std::string_view options, std::string_view type) {
    return "36 25 " + std::string(device) + " " + std::string(root) + " " + Escaped(mount) + " " + std::string(options) +
           " shared:1 - " + std::string(type) + " /dev/fake rw\n";
}

std::string FirstLine(const FakeHost& host) {
    return MountLine("8_17", "/", host.firstMount, "rw,nosuid,relatime", "vfat");
}

std::string SecondLine(const FakeHost& host) {
    return MountLine("8_33", "/", host.secondMount, "ro,relatime", "exfat");
}

constexpr std::uint64_t FirstVolumeSize = 61530439680ull;
constexpr std::uint64_t SecondVolumeSize = 31914983424ull;

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

void Inject(const FakeHost& host, bool first, bool second, bool firstMoved = false) {
    static const UsbStorageTestVolume none{};
    const auto firstRoot = Utf8(firstMoved ? host.movedMount : host.firstMount);
    const auto secondRoot = Utf8(host.secondMount);
    std::vector<UsbStorageTestVolume> volumes;
    if (first) volumes.push_back({"first", firstRoot.c_str(), FirstVolumeSize, 0x0781, 0x5581, 0x0100, "SanDisk", "Ultra Fit", "4C530001", false, false});
    if (second) volumes.push_back({"second", secondRoot.c_str(), SecondVolumeSize, 0x090c, 0x1000, 0x1100, "Samsung", "Flash Drive", "", true, true});
    SetUsbStorageHostVolumesForTest_nid_no_patch(volumes.empty() ? &none : volumes.data(), volumes.size());
}

void SetPassthrough(bool enabled) {
#ifdef _WIN32
    Require(_putenv_s("APS5_USB_STORAGE_PASSTHROUGH", enabled ? "1" : "") == 0, "set the passthrough switch");
#else
    Require((enabled ? ::setenv("APS5_USB_STORAGE_PASSTHROUGH", "1", 1) : ::unsetenv("APS5_USB_STORAGE_PASSTHROUGH")) == 0, "set the passthrough switch");
#endif
}

FakeHost CreateHost() {
    FakeHost host;
    host.root = std::filesystem::temp_directory_path() / ("anyps5_usb_storage_" + std::to_string(std::random_device{}()));
    host.mountInfo = host.root / "mountinfo";
    host.sys = host.root / "sys";
    host.firstMount = host.root / "media" / "USB DRIVE";
    host.secondMount = host.root / "media" / "second";
    host.movedMount = host.root / "media" / "moved";
    WriteFile(host.firstMount / "song.txt", "song");
    WriteFile(host.movedMount / "moved.txt", "moved");
    WriteFile(host.firstMount / ".hidden", "hidden");
    std::filesystem::create_directories(host.firstMount / "Music");
    WriteFile(host.secondMount / "Readme.txt", "readme");
    WriteFile(host.root / "media" / "efi" / "boot.efi", "efi");
    Inject(host, false, false);
    return host;
}

bool Link(const FakeHost& host, std::string_view device, const std::filesystem::path& target) {
    std::filesystem::create_directories(target);
    std::error_code error;
    std::filesystem::create_directory_symlink(target, host.sys / "dev" / "block" / std::string(device), error);
    return !error;
}

bool CreateSysfs(const FakeHost& host) {
    std::filesystem::create_directories(host.sys / "dev" / "block");
    if (!Link(host, "0_0", host.sys / "devices" / "probe")) return false;
    const auto controller = host.sys / "devices" / "pci0000_00" / "0000_00_14.0" / "usb1";
    const auto first = controller / "1-1";
    WriteFile(first / "idVendor", "0781\n");
    WriteFile(first / "idProduct", "5581\n");
    WriteFile(first / "bcdDevice", "0100\n");
    WriteFile(first / "serial", "4C530001\n");
    WriteFile(first / "1-1_1.0" / "bInterfaceClass", "08\n");
    const auto firstScsi = first / "1-1_1.0" / "host6" / "target6_0_0" / "6_0_0_0";
    WriteFile(firstScsi / "vendor", "SanDisk \n");
    WriteFile(firstScsi / "model", "Ultra Fit       \n");
    const auto second = controller / "1-2";
    WriteFile(second / "idVendor", "090c\n");
    WriteFile(second / "idProduct", "1000\n");
    WriteFile(second / "bcdDevice", "1100\n");
    WriteFile(second / "1-2_1.0" / "bInterfaceClass", "08\n");
    const auto secondScsi = second / "1-2_1.0" / "host7" / "target7_0_0" / "7_0_0_0";
    WriteFile(secondScsi / "vendor", "Samsung \n");
    WriteFile(secondScsi / "model", "Flash Drive     \n");
    const auto sata = host.sys / "devices" / "pci0000_00" / "0000_00_17.0" / "ata1" / "host0" / "target0_0_0" / "0_0_0_0";
    WriteFile(sata / "vendor", "ATA     \n");
    WriteFile(sata / "model", "Disk            \n");
    return Link(host, "8_17", firstScsi / "block" / "sdb" / "sdb1") && Link(host, "8_33", secondScsi / "block" / "sdc" / "sdc1") &&
           Link(host, "8_1", sata / "block" / "sda" / "sda1") && Link(host, "8_49", firstScsi / "block" / "sdd" / "sdd1");
}

void TestInitializeAndTerminate() {
    Require(sceUsbStorageTerm() == 0, "terminate before initialize");
    Require(sceUsbStorageInit(nullptr) == 0, "initialize with a null attribute");
    Require(sceUsbStorageInit(nullptr) == 0, "initialize again");
    Require(sceUsbStorageTerm() == 0, "terminate");
    Require(sceUsbStorageTerm() == 0, "terminate again");
    Require(sceUsbStorageInit(nullptr) == 0, "initialize after terminate");
    Require(sceUsbStorageTerm() == 0, "terminate after initializing again");
}

void TestRegisterCallbacks() {
    Require(ThrowsWith([] { sceUsbStorageRegisterCallback(1, Callback(), nullptr); },
                       "sceUsbStorageRegisterCallback: registering before sceUsbStorageInit is not implemented"),
            "register a callback before initialize did not throw");
    Require(ThrowsWith([] { sceUsbStorageUnregisterCallback(1, Callback()); }, "sceUsbStorageUnregisterCallback not implemented"),
            "unregister a callback before initialize did not throw its marker");
    Require(sceUsbStorageRegisterCallback(1, nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null callback");
    Require(sceUsbStorageRegisterCallback(0, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of event type 0");
    Require(sceUsbStorageRegisterCallback(3, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of event type 3");
    Require(sceUsbStorageRegisterCallback(-1, Callback(), nullptr) == UsbStorage::InvalidArgument, "register callback of a negative event type");
    Require(sceUsbStorageInit(nullptr) == 0, "initialize for callbacks");
    Require(sceUsbStorageRegisterCallback(1, Callback(), nullptr) == 0, "register a mount callback");
    Require(sceUsbStorageRegisterCallback(2, Callback(), &g_attr) == 0, "register an unmount callback");
    Require(ThrowsWith([] { sceUsbStorageRegisterCallback(1, Callback(), &g_attr); },
                       "sceUsbStorageRegisterCallback: registering a registered callback again is not implemented"),
            "register a registered callback again did not throw");
    Require(sceUsbStorageUnregisterCallback(1, nullptr) == UsbStorage::InvalidArgument, "unregister a null callback");
    Require(sceUsbStorageUnregisterCallback(0, Callback()) == UsbStorage::InvalidArgument, "unregister callback of event type 0");
    Require(sceUsbStorageUnregisterCallback(1, Callback()) == 0, "unregister the mount callback");
    Require(sceUsbStorageUnregisterCallback(2, Callback()) == 0, "unregister the unmount callback");
    Require(ThrowsWith([] { sceUsbStorageUnregisterCallback(1, Callback()); }, "sceUsbStorageUnregisterCallback not implemented"),
            "unregister an unregistered callback did not throw its marker");
    Require(sceUsbStorageRegisterCallback(2, Callback(), nullptr) == 0, "register a callback before terminate");
    Require(sceUsbStorageTerm() == 0, "terminate with a registered callback");
    Require(ThrowsWith([] { sceUsbStorageUnregisterCallback(2, Callback()); }, "sceUsbStorageUnregisterCallback not implemented"),
            "terminate kept the registered callback");
}

void TestMapCallbacks() {
    Require(sceUsbStorageRegisterCallbackForMapAvailable(0, nullptr, Callback(), nullptr) == UsbStorage::InvalidArgument, "register map-available callback with a null directory");
    Require(sceUsbStorageRegisterCallbackForMapAvailable(0, "/data", nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null map-available callback");
    Require(sceUsbStorageUnregisterCallbackForMapAvailable(0, nullptr, Callback()) == UsbStorage::InvalidArgument, "unregister map-available callback with a null directory");
    Require(sceUsbStorageUnregisterCallbackForMapAvailable(0, "/data", nullptr) == UsbStorage::InvalidArgument, "unregister a null map-available callback");
    Require(sceUsbStorageRegisterCallbackForMapUnavailable(0, nullptr, Callback(), nullptr) == UsbStorage::InvalidArgument, "register map-unavailable callback with a null directory");
    Require(sceUsbStorageRegisterCallbackForMapUnavailable(0, "/data", nullptr, nullptr) == UsbStorage::InvalidArgument, "register a null map-unavailable callback");
    Require(sceUsbStorageUnregisterCallbackForMapUnavailable(0, nullptr, Callback()) == UsbStorage::InvalidArgument, "unregister map-unavailable callback with a null directory");
    Require(sceUsbStorageUnregisterCallbackForMapUnavailable(0, "/data", nullptr) == UsbStorage::InvalidArgument, "unregister a null map-unavailable callback");
}

void TestNoDevice() {
    Require(sceUsbStorageInit(nullptr) == 0, "initialize without a device");
    std::uint32_t devices[4]{0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a, 0x5a5a5a5a};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0, "get the device list");
    Require(count == 0, "no usb storage device is attached");
    Require(devices[0] == 0x5a5a5a5a && devices[3] == 0x5a5a5a5a, "the device list was written");
    Require(sceUsbStorageGetDeviceList(nullptr, &count) == UsbStorage::InvalidArgument, "get the device list without a device array");
    Require(sceUsbStorageGetDeviceList(devices, nullptr) == UsbStorage::InvalidArgument, "get the device list without a count");

    UsbStorageDeviceInfo info;
    std::memset(&info, 0x5a, sizeof(info));
    Require(sceUsbStorageGetDeviceInfo(0, &info) == UsbStorage::DeviceNotFound, "get device info of device 0");
    Require(sceUsbStorageGetDeviceInfo(1, &info) == UsbStorage::DeviceNotFound, "get device info of device 1");
    Require(info.volumeSize == 0x5a5a5a5a5a5a5a5aull && info.flags == 0x5a5a5a5a5a5a5a5aull, "device info was written");
    Require(sceUsbStorageGetDeviceInfo(0, nullptr) == UsbStorage::InvalidArgument, "get device info without an info buffer");

    std::uint8_t exists = 1;
    Require(sceUsbStorageIsExist(0, "/mnt/usb0", &exists) == true && exists == 0, "device 0 does not exist");
    exists = 1;
    Require(sceUsbStorageIsExist(1, nullptr, &exists) == true && exists == 0, "device 1 does not exist");
    exists = 1;
    Require(sceUsbStorageIsExist(0xffffffffu, "/mnt/usb0", &exists) == true && exists == 0, "no device id exists");
    Require(ThrowsWith([] { sceUsbStorageIsExist(0, "/mnt/usb0", nullptr); }, "sceUsbStorageIsExist: a null output is not implemented"),
            "an existence query without an output did not throw");
    Require(sceUsbStorageTerm() == 0, "terminate without a device");
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
    Require(ThrowsWith([] { sceUsbStorageRequestUnmap(0, "/mnt/usb0"); }, "sceUsbStorageRequestUnmap not implemented"),
            "unmap a directory did not throw its marker");
    Require(ThrowsWith([] { sceUsbStorageRequestUnmap(7, "/mnt/usb7"); }, "sceUsbStorageRequestUnmap not implemented"),
            "unmap a directory of another device did not throw its marker");
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

    Require(ThrowsWith([] { sceUsbStorageGetdentsClose(1); }, "sceUsbStorageGetdentsClose not implemented"),
            "close a getdents scan that was never opened did not throw its marker");
    Require(ThrowsWith([] { sceUsbStorageGetdentsClose(0); }, "sceUsbStorageGetdentsClose not implemented"),
            "close descriptor 0 did not throw its marker");
}

void TestThrowingExports() {
    char mountPoint[8]{};
    Require(ThrowsWith([&] { sceUsbStorageGetMountPointOfShellCore(0, mountPoint); }, "sceUsbStorageGetMountPointOfShellCore not implemented"),
            "get the mount point of shell core without a device did not throw its marker");
    Require(ThrowsWith([] { sceUsbStorageClearFakeWriteProtection(); }, "sceUsbStorageClearFakeWriteProtection not implemented"),
            "clear fake write protection did not throw its marker");
    Require(ThrowsWith([] { sceUsbStorageSetFakeMapLockForDebug(); }, "sceUsbStorageSetFakeMapLockForDebug not implemented"),
            "set fake map lock for debug did not throw its marker");
    Require(ThrowsWith([] { sceUsbStorageSetFakeWriteProtection(); }, "sceUsbStorageSetFakeWriteProtection not implemented"),
            "set fake write protection did not throw its marker");
}

void TestDeviceInfo(const FakeHost& host) {
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0, "get the device list with a device");
    Require(count == 1 && devices[0] == 0, "the attached volume is listed as device 0");

    UsbStorageDeviceInfo info;
    std::memset(&info, 0x5a, sizeof(info));
    Require(sceUsbStorageGetDeviceInfo(0, &info) == 0, "get device info of the attached device");
    Require(info.vendorId == 0x0781 && info.productId == 0x5581 && info.bcdDevice == 0x0100, "the device info holds the USB ids and release");
    Require(std::string_view(info.vendor) == "SanDisk" && info.vendorLength == 7, "the device info holds the vendor");
    Require(std::string_view(info.product) == "Ultra Fit" && info.productLength == 9, "the device info holds the product");
    Require(std::string_view(info.serialNumber) == "4C530001" && info.serialNumberLength == 8, "the device info holds the serial number");
    Require(info.flags == UsbStorage::FatFlag, "the device info flags a writable FAT volume");
    Require(info.reserved == 0 && info.padding[0] == 0 && info.padding[1] == 0, "the device info cleared its unknown fields");
    Require(info.volumeSize == FirstVolumeSize, "the device info holds the volume size");
    Require(sceUsbStorageGetDeviceInfo(1, &info) == UsbStorage::DeviceNotFound, "get device info of an absent device");

    char mountPoint[UsbStorage::MountPointSize];
    std::memset(mountPoint, 0x5a, sizeof(mountPoint));
    Require(sceUsbStorageGetMountPointOfShellCore(0, mountPoint) == 0 && std::string_view(mountPoint) == "/mnt/usb0", "the device is mounted at /mnt/usb0");
    std::uint8_t exists = 0;
    Require(sceUsbStorageIsExist(0, "/", &exists) == true && exists == 1, "the root of the attached device exists");
    exists = 0;
    Require(sceUsbStorageIsExist(0, nullptr, &exists) == true && exists == 1, "a null directory is the root of the attached device");
    exists = 0;
    Require(sceUsbStorageIsExist(0, "Music/", &exists) == true && exists == 1, "a directory of the attached device exists");
    exists = 1;
    Require(sceUsbStorageIsExist(0, "/Missing", &exists) == true && exists == 0, "a missing directory does not exist");
    exists = 1;
    Require(sceUsbStorageIsExist(0, "/song.txt", &exists) == true && exists == 0, "a file is not a directory");
    Require(ThrowsWith([&] { sceUsbStorageIsExist(0, "/Music/../Music", &exists); }, "sceUsbStorageIsExist: a directory with . or .. components is not implemented"),
            "an existence query with a .. component did not throw");
    exists = 1;
    Require(sceUsbStorageIsExist(1, "/", &exists) == true && exists == 0, "device 1 does not exist");
    const auto resolved = ResolvePath_nid_no_patch("/mnt/usb0/song.txt");
    std::error_code error;
    Require(std::filesystem::equivalent(resolved, host.firstMount / "song.txt", error), "the mount point resolves to the host volume");
}

void TestDeviceMaps() {
    char mountPoint[UsbStorage::MountPointSize];
    std::uint64_t mountId = 0x5a5a5a5a5a5a5a5aull;
    Require(sceUsbStorageRequestMap(0, "/", 0, 0, mountPoint, &mountId) == 0, "map the root of the device");
    Require(std::string_view(mountPoint) == "/mnt/usb0" && mountId == 0, "the root is mapped at the mount point");
    Require(sceUsbStorageRequestMap(0, "Music/", 0, 0, mountPoint, nullptr) == 0, "map a directory of the device");
    Require(std::string_view(mountPoint) == "/mnt/usb0/Music", "the directory is mapped below the mount point");
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "/Missing", 0, 0, mountPoint, nullptr); },
                       "sceUsbStorageRequestMap: mapping a directory the device does not hold is not implemented"),
            "map a missing directory did not throw");
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "/song.txt", 0, 0, mountPoint, nullptr); },
                       "sceUsbStorageRequestMap: mapping a directory the device does not hold is not implemented"),
            "map a file did not throw");
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "/Music/../Music", 0, 0, mountPoint, nullptr); },
                       "sceUsbStorageRequestMap: a directory with . or .. components is not implemented"),
            "map a directory with a .. component did not throw");
#ifdef _WIN32
    constexpr std::string_view otherDrive = "sceUsbStorageRequestMap: a directory with a root name is not implemented";
#else
    constexpr std::string_view otherDrive = "sceUsbStorageRequestMap: mapping a directory the device does not hold is not implemented";
#endif
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "C:/Windows", 0, 0, mountPoint, nullptr); }, otherDrive),
            "map a directory of another drive did not throw");
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "/C:Windows", 0, 0, mountPoint, nullptr); }, otherDrive),
            "map a drive-relative directory did not throw");
    Require(ThrowsWith([&] { sceUsbStorageRequestMap(0, "Music\\..", 0, 0, mountPoint, nullptr); },
                       "sceUsbStorageRequestMap: a directory with a backslash is not implemented"),
            "map a directory with a backslash did not throw");
    Require(sceUsbStorageRequestUnmap(0, "/mnt/usb0/Music") == 0, "unmap the directory");
    Require(ThrowsWith([] { sceUsbStorageRequestUnmap(0, "/mnt/usb0/Music"); }, "sceUsbStorageRequestUnmap not implemented"),
            "unmap an unmapped directory did not throw its marker");
    Require(sceUsbStorageRequestUnmap(0, "/mnt/usb0") == 0, "unmap the root");
    std::int32_t status = 0x5a5a5a5a;
    Require(sceUsbStorageRequestMapWSB(0, nullptr, 0, 0, mountPoint, &status) == 0, "map the root through the WSB request");
    Require(std::string_view(mountPoint) == "/mnt/usb0" && status == 0, "the WSB request mapped the root at the mount point");
    Require(sceUsbStorageRequestUnmap(0, "/mnt/usb0") == 0, "unmap the WSB root");
}

void TestDeviceGetdents() {
    std::int32_t fd = -1;
    std::uint64_t bufferSize = 0;
    Require(sceUsbStorageGetdentsOpen(0, &fd, &bufferSize) == 0 && fd > 0 && bufferSize == 0x8000, "open a getdents scan of the device");
    std::uint64_t bytesRead = 0;
    std::vector<std::uint8_t> buffer(0x8000, 0x5a);
    Require(sceUsbStorageGetdentsRead(fd, buffer.data(), buffer.size(), &bytesRead) == 0, "read the getdents scan");
    std::set<std::tuple<std::string, std::uint8_t>> entries;
    std::set<std::uint32_t> fileNumbers;
    for (std::uint64_t offset = 0; offset < bytesRead;) {
        std::uint32_t fileNumber = 0;
        std::uint16_t length = 0;
        std::memcpy(&fileNumber, buffer.data() + offset, sizeof(fileNumber));
        std::memcpy(&length, buffer.data() + offset + 4, sizeof(length));
        const std::uint8_t type = buffer[offset + 6];
        const std::uint8_t nameLength = buffer[offset + 7];
        const std::string name(reinterpret_cast<const char*>(buffer.data() + offset + 8), nameLength);
        Require(length == ((8 + nameLength + 1 + 7) & ~7) && buffer[offset + 8 + nameLength] == 0, "a dirent record has the packed layout");
        Require(offset + length <= bytesRead, "a dirent record ends inside the bytes read");
        fileNumbers.insert(fileNumber);
        entries.emplace(name, type);
        offset += length;
    }
    Require(entries == std::set<std::tuple<std::string, std::uint8_t>>{{"Music", 4}, {"song.txt", 8}}, "the scan lists the root without dot names");
    Require(fileNumbers == std::set<std::uint32_t>{1, 2}, "the scan numbers its entries from 1");
    Require(sceUsbStorageGetdentsRead(fd, buffer.data(), buffer.size(), &bytesRead) == 0 && bytesRead == 0, "the scan ends with zero bytes");
    Require(sceUsbStorageGetdentsClose(fd) == 0, "close the getdents scan");
    Require(ThrowsWith([&] { sceUsbStorageGetdentsClose(fd); }, "sceUsbStorageGetdentsClose not implemented"), "close a closed scan did not throw its marker");
}

void TestPassthroughOff(const FakeHost& host) {
    SetPassthrough(false);
    Inject(host, true, false);
    Require(sceUsbStorageInit(nullptr) == 0, "initialize with the passthrough off");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 0, "initialize listed a host volume with the passthrough off");
    Require(sceUsbStorageRegisterCallback(UsbStorage::MountEvent, &Record, &g_mountTag) == 0, "register the mount recorder with the passthrough off");
    const auto seen = RecordedCount();
    Inject(host, true, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    Require(RecordedCount() == seen, "a rescan raised a mount event with the passthrough off");
    count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 0, "a rescan listed a host volume with the passthrough off");
    UsbStorageDeviceInfo info{};
    Require(sceUsbStorageGetDeviceInfo(0, &info) == UsbStorage::DeviceNotFound, "device 0 has info with the passthrough off");
    std::uint8_t exists = 1;
    Require(sceUsbStorageIsExist(0, nullptr, &exists) == true && exists == 0, "device 0 exists with the passthrough off");
    Require(sceUsbStorageRequestMap(0, nullptr, 0, 0, nullptr, nullptr) == UsbStorage::DeviceNotFound, "device 0 maps with the passthrough off");
    std::error_code error;
    Require(!std::filesystem::equivalent(ResolvePath_nid_no_patch("/mnt/usb0/song.txt"), host.firstMount / "song.txt", error),
            "/mnt/usb0 reaches a host volume with the passthrough off");
    Require(sceUsbStorageUnregisterCallback(UsbStorage::MountEvent, &Record) == 0, "unregister the mount recorder with the passthrough off");
    Require(sceUsbStorageTerm() == 0, "terminate with the passthrough off");
}

void TestDevices(const FakeHost& host) {
    Inject(host, true, false);
    Require(sceUsbStorageInit(nullptr) == 0, "initialize with a device");
    TestDeviceInfo(host);
    TestDeviceMaps();
    TestDeviceGetdents();
    Require(sceUsbStorageTerm() == 0, "terminate with a device");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 0, "terminate released the device list");
    std::error_code error;
    Require(!std::filesystem::equivalent(ResolvePath_nid_no_patch("/mnt/usb0/song.txt"), host.firstMount / "song.txt", error),
            "terminate removed the mount point");
}

void TestHotplug(const FakeHost& host) {
    Inject(host, true, false);
    Require(sceUsbStorageInit(nullptr) == 0, "initialize for hotplug");
    Require(sceUsbStorageRegisterCallback(UsbStorage::MountEvent, &Record, &g_mountTag) == 0, "register the mount recorder");
    Require(sceUsbStorageRegisterCallback(UsbStorage::UnmountEvent, &Record, &g_unmountTag) == 0, "register the unmount recorder");

    Inject(host, true, true);
    auto recorded = NextEvent(0);
    Require(recorded.event.event == 1 && recorded.event.device == 1 && recorded.userData == &g_mountTag, "a mounted device raises a mount event");
    Require(recorded.event.flags == (UsbStorage::ExFatFlag | UsbStorage::ReadOnlyFlag), "the mount event flags a read-only exFAT volume");
    UsbStorageDeviceInfo info{};
    Require(sceUsbStorageGetDeviceInfo(1, &info) == 0 && info.vendorId == 0x090c && info.bcdDevice == 0x1100, "the mounted device is listed");
    Require(info.serialNumber[0] == '\0' && info.serialNumberLength == 0 && std::string_view(info.product) == "Flash Drive", "a device without a serial number reports an empty one");

    Inject(host, false, true);
    recorded = NextEvent(1);
    Require(recorded.event.event == 2 && recorded.event.device == 0 && recorded.userData == &g_unmountTag, "a removed device raises an unmount event");
    Require(recorded.event.flags == UsbStorage::FatFlag, "the unmount event flags the removed volume");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 1 && devices[0] == 1, "the remaining device keeps its id");

    Inject(host, true, true);
    recorded = NextEvent(2);
    Require(recorded.event.event == 1 && recorded.event.device == 0, "a device mounted again takes the lowest free id");
    Require(sceUsbStorageUnregisterCallback(UsbStorage::MountEvent, &Record) == 0, "unregister the mount recorder");
    Require(sceUsbStorageUnregisterCallback(UsbStorage::UnmountEvent, &Record) == 0, "unregister the unmount recorder");
    Require(sceUsbStorageTerm() == 0, "terminate after hotplug");
    std::lock_guard lock(Events().mutex);
    Require(Events().events.size() == 3, "hotplug raised no other event");
}

void TestRootChange(const FakeHost& host) {
    Inject(host, true, false);
    Require(sceUsbStorageInit(nullptr) == 0, "initialize for a root change");
    std::error_code error;
    Require(std::filesystem::equivalent(ResolvePath_nid_no_patch("/mnt/usb0/song.txt"), host.firstMount / "song.txt", error), "the volume starts at its first root");
    Require(sceUsbStorageRegisterCallback(UsbStorage::MountEvent, &Record, &g_mountTag) == 0, "register the mount recorder for a root change");
    std::size_t seen = 0;
    {
        std::lock_guard lock(Events().mutex);
        seen = Events().events.size();
    }
    Inject(host, true, true, true);
    Require(NextEvent(seen).event.device == 1, "the scan that moved the volume mounted the second one");
    Require(std::filesystem::equivalent(ResolvePath_nid_no_patch("/mnt/usb0/moved.txt"), host.movedMount / "moved.txt", error),
            "the mount point follows a volume whose key stayed while its root changed");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 2 && devices[0] == 0, "the moved volume keeps its id");
    Require(sceUsbStorageUnregisterCallback(UsbStorage::MountEvent, &Record) == 0, "unregister the mount recorder after a root change");
    Require(sceUsbStorageTerm() == 0, "terminate after a root change");
}

void TestUnregisterDuringDispatch(const FakeHost& host) {
    Inject(host, false, false);
    Require(sceUsbStorageInit(nullptr) == 0, "initialize for unregistrations during dispatch");
    Require(sceUsbStorageRegisterCallback(UsbStorage::MountEvent, &RemoveSelf, nullptr) == 0, "register a callback that unregisters itself");
    Require(sceUsbStorageRegisterCallback(UsbStorage::MountEvent, &Record, &g_mountTag) == 0, "register the mount recorder after it");
    Require(sceUsbStorageRegisterCallback(UsbStorage::UnmountEvent, &BlockInside, nullptr) == 0, "register a callback that blocks");
    Require(sceUsbStorageRegisterCallback(UsbStorage::UnmountEvent, &Record, &g_unmountTag) == 0, "register the unmount recorder after it");
    const auto seen = RecordedCount();

    Inject(host, true, false);
    Require(NextEvent(seen).event.event == 1, "the first mount reached the recorder");
    Require(g_selfRemovalCalls == 1 && g_selfRemovalResult == 0, "a callback unregistered itself without deadlocking");
    Inject(host, true, true);
    Require(NextEvent(seen + 1).event.device == 1, "the second mount reached the recorder");
    Require(g_selfRemovalCalls == 1, "a callback that unregistered itself was called again");

    Inject(host, false, true);
    auto& blocker = Blocked();
    {
        std::unique_lock lock(blocker.mutex);
        Require(blocker.changed.wait_for(lock, std::chrono::seconds(10), [&] { return blocker.entered; }), "the blocking callback was not called");
    }
    std::atomic<bool> unregistering{false};
    std::atomic<bool> returned{false};
    std::atomic<std::int32_t> result{-1};
    std::thread other([&] {
        unregistering = true;
        result = sceUsbStorageUnregisterCallback(UsbStorage::UnmountEvent, &BlockInside);
        returned = true;
    });
    while (!unregistering) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(!returned, "an unregistration returned while its callback was still running");
    {
        std::lock_guard lock(blocker.mutex);
        blocker.released = true;
    }
    blocker.changed.notify_all();
    other.join();
    Require(returned && result == 0, "the unregistration returned after its callback");
    Require(NextEvent(seen + 2).event.device == 0, "the first unmount reached the recorder");
    Inject(host, false, false);
    Require(NextEvent(seen + 3).event.device == 1, "the second unmount reached the recorder");
    {
        std::lock_guard lock(blocker.mutex);
        Require(blocker.calls == 1, "a callback was called after its unregistration returned");
    }
    Require(sceUsbStorageUnregisterCallback(UsbStorage::MountEvent, &Record) == 0, "unregister the mount recorder after dispatch");
    Require(sceUsbStorageUnregisterCallback(UsbStorage::UnmountEvent, &Record) == 0, "unregister the unmount recorder after dispatch");
    Require(sceUsbStorageTerm() == 0, "terminate after unregistrations during dispatch");
}

void TestSysfsScan(const FakeHost& host) {
    WriteMounts(host, FirstLine(host) + SecondLine(host) + MountLine("8_1", "/", host.root / "media" / "efi", "rw", "vfat") +
                          MountLine("8_49", "/", host.root / "media", "rw", "ext4") + MountLine("8_17", "/Music", host.root / "media" / "bound", "rw", "vfat"));
    SetUsbStorageHostRootsForTest_nid_no_patch(Utf8(host.mountInfo).c_str(), Utf8(host.sys).c_str());
    Require(sceUsbStorageInit(nullptr) == 0, "initialize over the fake sysfs");
    std::uint32_t devices[UsbStorage::MaxDevices]{};
    std::int32_t count = -1;
    Require(sceUsbStorageGetDeviceList(devices, &count) == 0 && count == 2 && devices[0] == 0 && devices[1] == 1,
            "only the whole FAT and exFAT volumes of USB mass-storage devices are listed");
    UsbStorageDeviceInfo info{};
    Require(sceUsbStorageGetDeviceInfo(0, &info) == 0, "get the info of the scanned FAT volume");
    Require(info.vendorId == 0x0781 && info.productId == 0x5581 && info.bcdDevice == 0x0100, "the scan reads the USB ids and release");
    Require(std::string_view(info.vendor) == "SanDisk" && std::string_view(info.product) == "Ultra Fit", "the scan reads the SCSI strings without trailing spaces");
    Require(std::string_view(info.serialNumber) == "4C530001" && info.flags == UsbStorage::FatFlag, "the scan reads the serial number and a writable FAT volume");
    std::error_code error;
    const auto capacity = std::filesystem::space(host.firstMount, error).capacity;
    Require(!error && info.volumeSize == capacity, "the scanned volume size is the capacity of the mounted file system");
    Require(sceUsbStorageGetDeviceInfo(1, &info) == 0, "get the info of the scanned exFAT volume");
    Require(info.vendorId == 0x090c && std::string_view(info.product) == "Flash Drive" && info.serialNumberLength == 0, "the scan reads a device without a serial number");
    Require(info.flags == (UsbStorage::ExFatFlag | UsbStorage::ReadOnlyFlag), "the scan reads a read-only exFAT mount");
    Require(sceUsbStorageTerm() == 0, "terminate over the fake sysfs");
}

}

int main() {
    const auto host = CreateHost();
    TestInitializeAndTerminate();
    TestRegisterCallbacks();
    TestMapCallbacks();
    TestNoDevice();
    TestMapRequests();
    TestGetdents();
    TestThrowingExports();
    TestPassthroughOff(host);
    SetPassthrough(true);
    TestDevices(host);
    TestHotplug(host);
    TestRootChange(host);
    TestUnregisterDuringDispatch(host);
    if (CreateSysfs(host)) {
        TestSysfsScan(host);
    } else {
        std::printf("USBSTORAGE: directory symbolic links are unavailable, the sysfs scan is not exercised\n");
    }
    SetUsbStorageHostVolumesForTest_nid_no_patch(nullptr, 0);
    std::error_code error;
    std::filesystem::remove_all(host.root, error);
    return 0;
}
