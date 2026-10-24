#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include "prx/libSceUsbStorage/UsbStorage.hpp"

extern "C" {

std::int32_t APS5_VABI sceUsbStorageInit(void* attr) {
    (void)attr;
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageTerm(void) {
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRegisterCallback(std::int32_t eventType, UsbStorageCallback callback, void* arg) {
    (void)arg;
    if (callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    if (eventType < UsbStorage::CallbackEventTypeFirst || eventType > UsbStorage::CallbackEventTypeLast) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageUnregisterCallback(std::int32_t eventType, UsbStorageCallback callback) {
    if (callback == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    if (eventType < UsbStorage::CallbackEventTypeFirst || eventType > UsbStorage::CallbackEventTypeLast) {
        return UsbStorage::InvalidArgument;
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
    (void)device;
    if (info == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    return UsbStorage::DeviceNotFound;
}

std::int32_t APS5_VABI sceUsbStorageGetDeviceList(std::uint32_t* devices, std::int32_t* count) {
    if (devices == nullptr || count == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    *count = 0;
    return 0;
}

int APS5_VABI sceUsbStorageGetMountPointOfShellCore(uint32_t device, char* mountPoint) {
    (void)device;
    (void)mountPoint;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceUsbStorageIsExist(std::uint32_t device, const char* directory, std::uint8_t* exists) {
    (void)device;
    (void)directory;
    *exists = 0;
    return true;
}

std::int32_t APS5_VABI sceUsbStorageRequestMap(std::uint32_t device, const char* directory, std::int32_t flags,
                                               std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId) {
    (void)device;
    (void)directory;
    (void)flags;
    (void)volumeSize;
    (void)mountPoint;
    (void)mountId;
    return UsbStorage::DeviceNotFound;
}

std::int32_t APS5_VABI sceUsbStorageRequestUnmap(std::uint32_t device, const char* directory) {
    (void)device;
    if (directory == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsOpen(std::uint32_t device, std::int32_t* fd, std::uint64_t* bufferSize) {
    (void)device;
    if (fd == nullptr || bufferSize == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    return UsbStorage::DeviceNotFound;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsRead(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead) {
    (void)fd;
    (void)bufferSize;
    if (buffer == nullptr || bytesRead == nullptr) {
        return UsbStorage::InvalidArgument;
    }
    *bytesRead = 0;
    return UsbStorage::NotSupported;
}

std::int32_t APS5_VABI sceUsbStorageGetdentsClose(std::int32_t fd) {
    (void)fd;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbStorageClearFakeWriteProtection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbStorageRequestMapWSB(std::uint32_t device, const char* subpath, std::int32_t flags,
                                                  std::int32_t unknown, char* mountPath, std::int32_t* status) {
    (void)device;
    (void)subpath;
    (void)flags;
    (void)unknown;
    (void)mountPath;
    (void)status;
    return UsbStorage::DeviceNotFound;
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
