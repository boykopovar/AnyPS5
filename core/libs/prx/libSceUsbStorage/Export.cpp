#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceUsbStorage/include/InventoryExports.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include "prx/libSceUsbStorage/UsbStorage.hpp"
#include "prx/libSceUsbStorage/include/HostVolumes.hpp"

static_assert(sizeof(UsbStorageEvent) == 16);
static_assert(offsetof(UsbStorageEvent, device) == 4);
static_assert(offsetof(UsbStorageEvent, flags) == 8);
static_assert(sizeof(UsbStorageDeviceInfo) == 0x338);
static_assert(offsetof(UsbStorageDeviceInfo, vendorId) == 0x10);
static_assert(offsetof(UsbStorageDeviceInfo, productId) == 0x12);
static_assert(offsetof(UsbStorageDeviceInfo, bcdDevice) == 0x14);
static_assert(offsetof(UsbStorageDeviceInfo, vendor) == 0x16);
static_assert(offsetof(UsbStorageDeviceInfo, vendorLength) == 0x118);
static_assert(offsetof(UsbStorageDeviceInfo, product) == 0x120);
static_assert(offsetof(UsbStorageDeviceInfo, productLength) == 0x220);
static_assert(offsetof(UsbStorageDeviceInfo, serialNumber) == 0x228);
static_assert(offsetof(UsbStorageDeviceInfo, serialNumberLength) == 0x328);
static_assert(offsetof(UsbStorageDeviceInfo, flags) == 0x330);

namespace {

constexpr std::uint64_t GetdentsBufferSize = 0x8000;
constexpr std::size_t DirentHeaderSize = 8;
constexpr std::size_t NameLimit = 255;
constexpr std::uint8_t DirectoryType = 4;
constexpr std::uint8_t RegularType = 8;
constexpr auto RescanInterval = std::chrono::seconds(1);

struct Device {
    UsbStorage::HostVolume volume;
    std::vector<std::string> maps;
};

struct Registration {
    std::int32_t eventType;
    UsbStorageCallback callback;
    void* arg;
};

struct DirectoryEntry {
    std::string name;
    bool directory;
};

struct DirectoryScan {
    std::vector<DirectoryEntry> entries;
    std::size_t next = 0;
};

struct Service {
    std::mutex mutex;
    std::mutex dispatch;
    std::condition_variable wake;
    bool initialized = false;
    bool stopping = false;
    Pthread monitor = nullptr;
    std::array<std::optional<Device>, UsbStorage::MaxDevices> devices;
    std::vector<Registration> registrations;
    std::map<std::int32_t, DirectoryScan> scans;
    std::int32_t nextDescriptor = 1;
};

Service& State() {
    static auto* service = new Service;
    return *service;
}

std::string MountPoint(std::size_t slot) {
    return "/mnt/usb" + std::to_string(slot);
}

std::uint64_t Flags(const UsbStorage::HostVolume& volume) {
    std::uint64_t flags = volume.fileSystem == UsbStorage::FileSystem::ExFat ? UsbStorage::ExFatFlag : UsbStorage::FatFlag;
    if (volume.readOnly) flags |= UsbStorage::ReadOnlyFlag;
    return flags;
}

UsbStorageEvent Event(std::int32_t type, std::size_t slot, const UsbStorage::HostVolume& volume) {
    return {static_cast<std::uint32_t>(type), static_cast<std::uint32_t>(slot), Flags(volume)};
}

Device* Find(Service& service, std::uint32_t device) {
    if (device >= service.devices.size() || !service.devices[device]) return nullptr;
    return &*service.devices[device];
}

void Update(Service& service, std::vector<UsbStorage::HostVolume> volumes, std::vector<UsbStorageEvent>& events) {
    for (std::size_t slot = 0; slot < service.devices.size(); ++slot) {
        auto& device = service.devices[slot];
        if (!device) continue;
        const auto found = std::find_if(volumes.begin(), volumes.end(), [&](const auto& volume) { return volume.key == device->volume.key; });
        if (found != volumes.end()) {
            if (found->root != device->volume.root) AddPathAlias_nid_no_patch(MountPoint(slot).c_str(), found->root.string().c_str());
            device->volume = std::move(*found);
            volumes.erase(found);
            continue;
        }
        events.push_back(Event(UsbStorage::UnmountEvent, slot, device->volume));
        RemovePathAlias_nid_no_patch(MountPoint(slot).c_str());
        device.reset();
    }
    for (auto& volume : volumes) {
        const auto free = std::find_if(service.devices.begin(), service.devices.end(), [](const auto& device) { return !device; });
        if (free == service.devices.end()) break;
        const auto slot = static_cast<std::size_t>(free - service.devices.begin());
        AddPathAlias_nid_no_patch(MountPoint(slot).c_str(), volume.root.string().c_str());
        events.push_back(Event(UsbStorage::MountEvent, slot, volume));
        free->emplace(Device{std::move(volume), {}});
    }
}

std::vector<Registration>::iterator Registered(Service& service, std::int32_t eventType, UsbStorageCallback callback) {
    return std::find_if(service.registrations.begin(), service.registrations.end(),
                        [&](const auto& registration) { return registration.eventType == eventType && registration.callback == callback; });
}

void Dispatch(Service& service, const std::vector<UsbStorageEvent>& events) {
    for (const auto& event : events) {
        std::vector<Registration> registrations;
        {
            std::lock_guard lock(service.mutex);
            registrations = service.registrations;
        }
        for (const auto& registration : registrations) {
            if (registration.eventType != static_cast<std::int32_t>(event.event)) continue;
            std::lock_guard calling(service.dispatch);
            void* arg = nullptr;
            {
                std::lock_guard lock(service.mutex);
                const auto current = Registered(service, registration.eventType, registration.callback);
                if (current == service.registrations.end()) continue;
                arg = current->arg;
            }
            registration.callback(&event, arg);
        }
    }
}

void* APS5_VABI Monitor(void*) {
    auto& service = State();
    std::unique_lock lock(service.mutex);
    while (!service.wake.wait_for(lock, RescanInterval, [&] { return service.stopping; })) {
        lock.unlock();
        auto volumes = UsbStorage::ScanHostVolumes();
        lock.lock();
        if (service.stopping) break;
        std::vector<UsbStorageEvent> events;
        Update(service, std::move(volumes), events);
        lock.unlock();
        Dispatch(service, events);
        lock.lock();
    }
    return nullptr;
}

std::filesystem::path HostName(std::string_view name) {
    return std::filesystem::path(std::u8string(name.begin(), name.end()));
}

std::vector<std::string> DirectoryNames(const char* directory, const char* function) {
    std::vector<std::string> names;
    if (directory == nullptr) return names;
    std::string_view text(directory);
    if (text.find('\\') != std::string_view::npos) throw std::runtime_error(std::string(function) + ": a directory with a backslash is not implemented");
    while (!text.empty()) {
        const auto separator = text.find('/');
        const auto name = text.substr(0, separator);
        if (name == "." || name == "..") throw std::runtime_error(std::string(function) + ": a directory with . or .. components is not implemented");
        if (HostName(name).has_root_path()) throw std::runtime_error(std::string(function) + ": a directory with a root name is not implemented");
        if (!name.empty()) names.emplace_back(name);
        if (separator == std::string_view::npos) break;
        text.remove_prefix(separator + 1);
    }
    return names;
}

std::filesystem::path HostDirectory(const Device& device, const std::vector<std::string>& names) {
    auto host = device.volume.root;
    for (const auto& name : names) host /= HostName(name);
    return host;
}

std::string Map(Device& device, std::uint32_t slot, const char* directory, const char* function) {
    const auto names = DirectoryNames(directory, function);
    auto path = MountPoint(slot);
    for (const auto& name : names) path += "/" + name;
    std::error_code error;
    if (!std::filesystem::is_directory(HostDirectory(device, names), error)) {
        throw std::runtime_error(std::string(function) + ": mapping a directory the device does not hold is not implemented");
    }
    if (path.size() >= UsbStorage::MountPointSize) throw std::runtime_error(std::string(function) + ": a mount point longer than 127 bytes is not implemented");
    if (std::find(device.maps.begin(), device.maps.end(), path) != device.maps.end()) {
        throw std::runtime_error(std::string(function) + ": mapping a mapped directory again is not implemented");
    }
    device.maps.push_back(path);
    return path;
}

void WriteMountPoint(char* mountPoint, const std::string& path) {
    if (mountPoint != nullptr) std::memcpy(mountPoint, path.c_str(), path.size() + 1);
}

std::uint64_t CopyString(char (&destination)[256], const std::string& text, const char* function) {
    if (text.size() >= sizeof(destination)) throw std::runtime_error(std::string(function) + ": a device string longer than 255 bytes is not implemented");
    std::memcpy(destination, text.data(), text.size());
    return text.size();
}

}

extern "C" {

int APS5_VABI sceUsbStorageClearFakeMapLockForDebug() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUsbStorageCheckSystemDefaultMountPoint() {
    NotImplemented_nid_no_patch(__func__);
}


std::int32_t APS5_VABI sceUsbStorageInit(void* attr) {
    if (attr != nullptr) throw std::runtime_error("sceUsbStorageInit: a thread attribute is not implemented");
    auto& service = State();
    {
        std::lock_guard lock(service.mutex);
        if (service.stopping) throw std::runtime_error("sceUsbStorageInit: initializing while sceUsbStorageTerm runs is not implemented");
        if (service.initialized) return 0;
    }
    auto volumes = UsbStorage::ScanHostVolumes();
    std::lock_guard lock(service.mutex);
    if (service.stopping) throw std::runtime_error("sceUsbStorageInit: initializing while sceUsbStorageTerm runs is not implemented");
    if (service.initialized) return 0;
    std::vector<UsbStorageEvent> events;
    Update(service, std::move(volumes), events);
    if (scePthreadCreate(&service.monitor, nullptr, Monitor, nullptr, "SceUsbStorageMonitor") != 0) {
        Update(service, {}, events);
        service.monitor = nullptr;
        throw std::runtime_error("sceUsbStorageInit: scePthreadCreate failed");
    }
    service.initialized = true;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageTerm(void) {
    auto& service = State();
    Pthread monitor = nullptr;
    {
        std::lock_guard lock(service.mutex);
        if (!service.initialized) return 0;
        if (service.stopping) throw std::runtime_error("sceUsbStorageTerm: a concurrent termination is not implemented");
        if (scePthreadEqual(scePthreadSelf(), service.monitor) != 0) {
            throw std::runtime_error("sceUsbStorageTerm: terminating from a USB storage callback is not implemented");
        }
        service.stopping = true;
        monitor = service.monitor;
    }
    service.wake.notify_all();
    if (scePthreadJoin(monitor, nullptr) != 0) throw std::runtime_error("sceUsbStorageTerm: scePthreadJoin failed");
    std::lock_guard lock(service.mutex);
    std::vector<UsbStorageEvent> events;
    Update(service, {}, events);
    service.registrations.clear();
    service.scans.clear();
    service.monitor = nullptr;
    service.stopping = false;
    service.initialized = false;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRegisterCallback(std::int32_t eventType, UsbStorageCallback callback, void* arg) {
    if (callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    if (eventType != UsbStorage::MountEvent && eventType != UsbStorage::UnmountEvent) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    if (!service.initialized) throw std::runtime_error("sceUsbStorageRegisterCallback: registering before sceUsbStorageInit is not implemented");
    if (Registered(service, eventType, callback) != service.registrations.end()) {
        throw std::runtime_error("sceUsbStorageRegisterCallback: registering a registered callback again is not implemented");
    }
    service.registrations.push_back({eventType, callback, arg});
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageUnregisterCallback(std::int32_t eventType, UsbStorageCallback callback) {
    if (callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    if (eventType != UsbStorage::MountEvent && eventType != UsbStorage::UnmountEvent) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    bool fromCallback = false;
    {
        std::lock_guard lock(service.mutex);
        fromCallback = service.monitor != nullptr && scePthreadEqual(scePthreadSelf(), service.monitor) != 0;
    }
    std::unique_lock calling(service.dispatch, std::defer_lock);
    if (!fromCallback) calling.lock();
    std::lock_guard lock(service.mutex);
    const auto found = Registered(service, eventType, callback);
    if (found != service.registrations.end()) {
        service.registrations.erase(found);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRegisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    (void)device;
    (void)arg;
    if (directory == nullptr || callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageUnregisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback) {
    (void)device;
    if (directory == nullptr || callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRegisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    (void)device;
    (void)arg;
    if (directory == nullptr || callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageUnregisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback) {
    (void)device;
    if (directory == nullptr || callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetDeviceInfo(std::uint32_t device, UsbStorageDeviceInfo* info) {
    if (info == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    const auto* found = Find(service, device);
    if (found == nullptr) {
        return UsbStorage::DeviceNotFound;
    }
    const auto& volume = found->volume;
    UsbStorageDeviceInfo record{};
    record.volumeSize = volume.volumeSize;
    record.vendorId = volume.vendorId;
    record.productId = volume.productId;
    record.bcdDevice = volume.bcdDevice;
    record.vendorLength = CopyString(record.vendor, volume.vendor, __func__);
    record.productLength = CopyString(record.product, volume.product, __func__);
    record.serialNumberLength = CopyString(record.serialNumber, volume.serialNumber, __func__);
    record.flags = Flags(volume);
    std::memcpy(info, &record, sizeof(record));
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetDeviceList(std::uint32_t* devices, std::int32_t* count) {
    if (devices == nullptr || count == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    std::int32_t listed = 0;
    for (std::size_t slot = 0; slot < service.devices.size(); ++slot) {
        if (service.devices[slot]) devices[listed++] = static_cast<std::uint32_t>(slot);
    }
    *count = listed;
    return 0;
}

int APS5_VABI sceUsbStorageGetMountPointOfShellCore(uint32_t device, char* mountPoint) {
    auto& service = State();
    std::lock_guard lock(service.mutex);
    if (Find(service, device) == nullptr || mountPoint == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    WriteMountPoint(mountPoint, MountPoint(device));
    return 0;
}

bool APS5_VABI sceUsbStorageIsExist(std::uint32_t device, const char* directory, std::uint8_t* exists) {
    if (exists == nullptr) throw std::runtime_error("sceUsbStorageIsExist: a null output is not implemented");
    auto& service = State();
    std::lock_guard lock(service.mutex);
    const auto* found = Find(service, device);
    std::error_code error;
    *exists = found != nullptr && std::filesystem::is_directory(HostDirectory(*found, DirectoryNames(directory, __func__)), error) ? 1 : 0;
    return true;
}

std::int32_t APS5_VABI sceUsbStorageRequestMap(std::uint32_t device, const char* directory, std::int32_t flags,
                                               std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId) {
    auto& service = State();
    std::lock_guard lock(service.mutex);
    auto* found = Find(service, device);
    if (found == nullptr) {
        return UsbStorage::DeviceNotFound;
    }
    if (flags != 0 || volumeSize != 0) throw std::runtime_error("sceUsbStorageRequestMap: nonzero flags or a volume size are not implemented");
    WriteMountPoint(mountPoint, Map(*found, device, directory, __func__));
    if (mountId != nullptr) *mountId = 0;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRequestUnmap(std::uint32_t device, const char* directory) {
    if (directory == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    if (auto* found = Find(service, device)) {
        const auto map = std::find(found->maps.begin(), found->maps.end(), std::string_view(directory));
        if (map != found->maps.end()) {
            found->maps.erase(map);
            return 0;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsOpen(std::uint32_t device, std::int32_t* fd, std::uint64_t* bufferSize) {
    if (fd == nullptr || bufferSize == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    const auto* found = Find(service, device);
    if (found == nullptr) {
        return UsbStorage::DeviceNotFound;
    }
    DirectoryScan scan;
    std::error_code error;
    for (std::filesystem::directory_iterator entry(found->volume.root, error), end; !error && entry != end; entry.increment(error)) {
        const auto name = entry->path().filename().u8string();
        if (name.empty() || name.front() == u8'.') continue;
        if (name.size() > NameLimit) throw std::runtime_error("sceUsbStorageGetdentsOpen: a name longer than 255 bytes is not implemented");
        std::error_code typeError;
        scan.entries.push_back({std::string(name.begin(), name.end()), entry->is_directory(typeError)});
    }
    if (error) throw std::runtime_error("sceUsbStorageGetdentsOpen: listing the device failed: " + error.message());
    if (service.nextDescriptor == std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("sceUsbStorageGetdentsOpen: the descriptor numbers are exhausted");
    const auto descriptor = service.nextDescriptor++;
    service.scans.emplace(descriptor, std::move(scan));
    *fd = descriptor;
    *bufferSize = GetdentsBufferSize;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsRead(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead) {
    if (buffer == nullptr || bytesRead == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    auto& service = State();
    std::lock_guard lock(service.mutex);
    const auto found = service.scans.find(fd);
    if (found == service.scans.end()) {
        *bytesRead = 0;
        return UsbStorage::NotSupported;
    }
    auto& scan = found->second;
    auto* const output = static_cast<std::uint8_t*>(buffer);
    std::uint64_t written = 0;
    while (scan.next < scan.entries.size()) {
        const auto& entry = scan.entries[scan.next];
        const auto length = static_cast<std::uint16_t>((DirentHeaderSize + entry.name.size() + 1 + 7) & ~std::size_t{7});
        if (length > bufferSize - written) break;
        auto* const record = output + written;
        const auto fileNumber = static_cast<std::uint32_t>(scan.next + 1);
        const auto type = entry.directory ? DirectoryType : RegularType;
        const auto nameLength = static_cast<std::uint8_t>(entry.name.size());
        std::memset(record, 0, length);
        std::memcpy(record, &fileNumber, sizeof(fileNumber));
        std::memcpy(record + 4, &length, sizeof(length));
        std::memcpy(record + 6, &type, sizeof(type));
        std::memcpy(record + 7, &nameLength, sizeof(nameLength));
        std::memcpy(record + DirentHeaderSize, entry.name.data(), entry.name.size());
        written += length;
        ++scan.next;
    }
    if (written == 0 && scan.next < scan.entries.size()) {
        throw std::runtime_error("sceUsbStorageGetdentsRead: a buffer smaller than the next entry is not implemented");
    }
    *bytesRead = written;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsClose(std::int32_t fd) {
    auto& service = State();
    std::lock_guard lock(service.mutex);
    if (service.scans.erase(fd) == 1) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbStorageClearFakeWriteProtection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRequestMapWSB(std::uint32_t device, const char* subpath, std::int32_t flags,
                                                  std::int32_t unknown, char* mountPath, std::int32_t* status) {
    auto& service = State();
    std::lock_guard lock(service.mutex);
    auto* found = Find(service, device);
    if (found == nullptr) {
        return UsbStorage::DeviceNotFound;
    }
    if (flags != 0 || unknown != 0) throw std::runtime_error("sceUsbStorageRequestMapWSB: nonzero flags or a nonzero fourth argument are not implemented");
    WriteMountPoint(mountPath, Map(*found, device, subpath, __func__));
    if (status != nullptr) *status = 0;
    return 0;
}

int APS5_VABI sceUsbStorageSetFakeMapLockForDebug() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbStorageSetFakeWriteProtection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceUsbStorageUnknown_267oD_minus_IF_plus_8k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("267oD-IF+8k", libSceUsbStorageUnknown_267oD_minus_IF_plus_8k_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_96s31EeG0LM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("96s31EeG0LM", libSceUsbStorageUnknown_96s31EeG0LM_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_AIyv_minus_u_minus_8w0s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AIyv-u-8w0s", libSceUsbStorageUnknown_AIyv_minus_u_minus_8w0s_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_FhdRv3K4EMQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FhdRv3K4EMQ", libSceUsbStorageUnknown_FhdRv3K4EMQ_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_H6axEciG5Gw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("H6axEciG5Gw", libSceUsbStorageUnknown_H6axEciG5Gw_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_IgVY6sHaVuE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IgVY6sHaVuE", libSceUsbStorageUnknown_IgVY6sHaVuE_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_O6_plus_fXKZS62g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("O6+fXKZS62g", libSceUsbStorageUnknown_O6_plus_fXKZS62g_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_OhsV6P0FD5A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OhsV6P0FD5A", libSceUsbStorageUnknown_OhsV6P0FD5A_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_PsB2k0hnCRo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PsB2k0hnCRo", libSceUsbStorageUnknown_PsB2k0hnCRo_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_W_plus_CjjL0iEmU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("W+CjjL0iEmU", libSceUsbStorageUnknown_W_plus_CjjL0iEmU_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_W5e8Vxa9OCI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("W5e8Vxa9OCI", libSceUsbStorageUnknown_W5e8Vxa9OCI_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_W6xoHROgu6w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("W6xoHROgu6w", libSceUsbStorageUnknown_W6xoHROgu6w_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_XMDFDnkOilA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XMDFDnkOilA", libSceUsbStorageUnknown_XMDFDnkOilA_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_d5QHvMfsCx0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("d5QHvMfsCx0", libSceUsbStorageUnknown_d5QHvMfsCx0_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_dVQFgpSzA2I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dVQFgpSzA2I", libSceUsbStorageUnknown_dVQFgpSzA2I_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_jjKZ79juYfE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jjKZ79juYfE", libSceUsbStorageUnknown_jjKZ79juYfE_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_jl_plus__plus_fwrnXXI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jl++fwrnXXI", libSceUsbStorageUnknown_jl_plus__plus_fwrnXXI_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_jpS2TJZhsRU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jpS2TJZhsRU", libSceUsbStorageUnknown_jpS2TJZhsRU_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_jqw6RT84rQI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jqw6RT84rQI", libSceUsbStorageUnknown_jqw6RT84rQI_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_kzxjYuKrhv0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kzxjYuKrhv0", libSceUsbStorageUnknown_kzxjYuKrhv0_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_rZgSodboauI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rZgSodboauI", libSceUsbStorageUnknown_rZgSodboauI_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_uXZqnG02MiM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uXZqnG02MiM", libSceUsbStorageUnknown_uXZqnG02MiM_nid_no_patch);

int APS5_VABI libSceUsbStorageUnknown_yct_minus_absP9LM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yct-absP9LM", libSceUsbStorageUnknown_yct_minus_absP9LM_nid_no_patch);

}
