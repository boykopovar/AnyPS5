#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_USB_STORAGE_ERROR_INVAL = static_cast<std::int32_t>(0x80F40002);
constexpr std::int32_t SCE_USB_STORAGE_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x80F40003);
constexpr std::int32_t SCE_USB_STORAGE_ERROR_NOENT = static_cast<std::int32_t>(0x80F40004);
constexpr std::int32_t SCE_USB_STORAGE_ERROR_NOTSUP = static_cast<std::int32_t>(0x80F40005);

struct SceUsbStorageDeviceInfo {
    std::uint32_t device_id;
    std::uint8_t pad0[12];
    std::uint16_t unk10;
    std::uint16_t unk12;
    std::uint16_t unk14;
    char vendor[256];
    std::uint64_t capacity;
    char product[256];
    std::uint64_t status;
    char mount_path[256];
    std::uint64_t ready;
    std::uint8_t pad1[48];
};

static_assert(sizeof(SceUsbStorageDeviceInfo) == 0x360);

struct SceUsbStorageDirent {
    std::uint32_t d_fileno;
    std::uint16_t d_reclen;
    std::uint8_t d_type;
    std::uint8_t d_namlen;
    char d_name[256];
};

struct UsbDirentItem {
    std::string name;
    bool is_directory;
};

struct UsbGetdentsState {
    std::vector<UsbDirentItem> items;
    std::size_t current_index = 0;
};

std::atomic<bool> g_initialized{false};
std::mutex g_getdentsMutex;
std::unordered_map<std::int32_t, UsbGetdentsState> g_getdentsTable;
std::int32_t g_nextFd = 1;

}

extern "C" {

int APS5_VABI sceUsbStorageInit(void* attr) {
    (void)attr;
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceUsbStorageTerm(void) {
    g_initialized.store(false);
    std::lock_guard lock(g_getdentsMutex);
    g_getdentsTable.clear();
    return 0;
}

int APS5_VABI sceUsbStorageGetDeviceList(uint32_t* device_ids, uint32_t* count) {
    if (!device_ids || !count) return SCE_USB_STORAGE_ERROR_INVAL;
    if (!g_initialized.load()) return SCE_USB_STORAGE_ERROR_NOT_INITIALIZED;
    *count = 0;
    return 0;
}

int APS5_VABI sceUsbStorageGetDeviceInfo(uint32_t device_id, SceUsbStorageDeviceInfo* info) {
    (void)device_id;
    if (!info) return SCE_USB_STORAGE_ERROR_INVAL;
    if (!g_initialized.load()) return SCE_USB_STORAGE_ERROR_NOT_INITIALIZED;
    return SCE_USB_STORAGE_ERROR_NOENT;
}

int APS5_VABI sceUsbStorageIsExist(uint32_t device_id) {
    (void)device_id;
    return 0;
}

int APS5_VABI sceUsbStorageGetMountPointOfShellCore(uint32_t device_id, char* mount_point, size_t max_len) {
    if (!mount_point || max_len == 0) return SCE_USB_STORAGE_ERROR_INVAL;
    if (device_id != 0) return SCE_USB_STORAGE_ERROR_NOENT;
    const char default_mount[] = "/mnt/usb0";
    if (max_len <= sizeof(default_mount)) return SCE_USB_STORAGE_ERROR_INVAL;
    std::memcpy(mount_point, default_mount, sizeof(default_mount));
    return 0;
}

int APS5_VABI sceUsbStorageGetdentsOpen(uint32_t device_id, int32_t* out_fd, uint64_t* out_bufsize) {
    (void)device_id;
    if (!out_fd || !out_bufsize) return SCE_USB_STORAGE_ERROR_INVAL;
    if (!g_initialized.load()) return SCE_USB_STORAGE_ERROR_NOT_INITIALIZED;
    return SCE_USB_STORAGE_ERROR_NOENT;
}

int APS5_VABI sceUsbStorageGetdentsRead(int32_t fd, void* buffer, uint64_t bufsize, uint64_t* out_bytes_read) {
    if (!buffer || !out_bytes_read) return SCE_USB_STORAGE_ERROR_INVAL;
    if (!g_initialized.load()) return SCE_USB_STORAGE_ERROR_NOT_INITIALIZED;
    std::lock_guard lock(g_getdentsMutex);
    auto it = g_getdentsTable.find(fd);
    if (it == g_getdentsTable.end()) {
        *out_bytes_read = 0;
        return SCE_USB_STORAGE_ERROR_INVAL;
    }
    auto& state = it->second;
    uint64_t bytes_written = 0;
    while (state.current_index < state.items.size()) {
        const auto& item = state.items[state.current_index];
        const uint16_t namlen = static_cast<uint16_t>(std::min<size_t>(item.name.size(), 255));
        const uint16_t reclen = static_cast<uint16_t>((8 + namlen + 1 + 7) & ~7);
        if (bytes_written + reclen > bufsize) break;
        auto* entry = reinterpret_cast<SceUsbStorageDirent*>(static_cast<uint8_t*>(buffer) + bytes_written);
        std::memset(entry, 0, reclen);
        entry->d_fileno = static_cast<uint32_t>(state.current_index + 1);
        entry->d_reclen = reclen;
        entry->d_type = item.is_directory ? 4 : 8;
        entry->d_namlen = static_cast<uint8_t>(namlen);
        std::memcpy(entry->d_name, item.name.data(), namlen);
        entry->d_name[namlen] = '\0';
        bytes_written += reclen;
        state.current_index++;
    }
    *out_bytes_read = bytes_written;
    return 0;
}

int APS5_VABI sceUsbStorageGetdentsClose(int32_t fd) {
    std::lock_guard lock(g_getdentsMutex);
    g_getdentsTable.erase(fd);
    return 0;
}

int APS5_VABI sceUsbStorageRegisterCallback(int32_t event_type, void* callback, void* arg) {
    (void)arg;
    if (!callback) return SCE_USB_STORAGE_ERROR_INVAL;
    if (event_type != 1 && event_type != 2) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageUnregisterCallback(int32_t event_type, void* callback) {
    if (!callback) return SCE_USB_STORAGE_ERROR_INVAL;
    if (event_type != 1 && event_type != 2) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageRegisterCallbackForMapAvailable(uint32_t device_id, const char* directory, void* callback, void* arg) {
    (void)device_id;
    (void)arg;
    if (!directory || !callback) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageUnregisterCallbackForMapAvailable(uint32_t device_id, const char* directory, void* callback) {
    (void)device_id;
    if (!directory || !callback) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageRegisterCallbackForMapUnavailable(uint32_t device_id, const char* directory, void* callback, void* arg) {
    (void)device_id;
    (void)arg;
    if (!directory || !callback) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageUnregisterCallbackForMapUnavailable(uint32_t device_id, const char* directory, void* callback) {
    (void)device_id;
    if (!directory || !callback) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageRequestMap(uint32_t device_id, const char* subpath, int32_t flags, int32_t unk, char* out_mount_path, int32_t* out_status) {
    (void)flags;
    (void)unk;
    if (device_id != 0) return SCE_USB_STORAGE_ERROR_NOENT;
    std::string mount_path = "/mnt/usb" + std::to_string(device_id);
    if (subpath && subpath[0] != '\0') {
        std::string_view sp(subpath);
        if (sp.starts_with("/mnt/usb")) {
            mount_path = std::string(sp);
        } else if (sp.starts_with("/")) {
            mount_path += std::string(sp);
        } else {
            mount_path += "/" + std::string(sp);
        }
    }
    if (out_mount_path) {
        std::strncpy(out_mount_path, mount_path.c_str(), 127);
        out_mount_path[127] = '\0';
    }
    if (out_status) *out_status = 0;
    return 0;
}

int APS5_VABI sceUsbStorageRequestMapWSB(uint32_t device_id, const char* subpath, int32_t flags, int32_t unk, char* out_mount_path, int32_t* out_status) {
    return sceUsbStorageRequestMap(device_id, subpath, flags, unk, out_mount_path, out_status);
}

int APS5_VABI sceUsbStorageRequestUnmap(uint32_t device_id, const char* directory) {
    (void)device_id;
    if (!directory) return SCE_USB_STORAGE_ERROR_INVAL;
    return 0;
}

int APS5_VABI sceUsbStorageClearFakeWriteProtection(uint32_t device_id) {
    (void)device_id;
    return 0;
}

int APS5_VABI sceUsbStorageSetFakeWriteProtection(uint32_t device_id, uint32_t enable) {
    (void)device_id;
    (void)enable;
    return 0;
}

int APS5_VABI sceUsbStorageSetFakeMapLockForDebug(uint32_t device_id, uint32_t lock) {
    (void)device_id;
    (void)lock;
    return 0;
}

}
