#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct UsbStorageEvent;
struct UsbStorageDeviceInfo;
using UsbStorageCallback = void (APS5_VABI *)(const UsbStorageEvent* event, void* userData);

extern "C" {

int APS5_VABI sceUsbStorageInit(void* attr) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageRegisterCallback(int32_t eventType, UsbStorageCallback callback, void* arg) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageUnregisterCallback(int32_t eventType, UsbStorageCallback callback) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageRegisterCallbackForMapAvailable(uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageUnregisterCallbackForMapAvailable(uint32_t device, const char* directory, UsbStorageCallback callback) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageRegisterCallbackForMapUnavailable(uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageUnregisterCallbackForMapUnavailable(uint32_t device, const char* directory, UsbStorageCallback callback) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetDeviceInfo(uint32_t device, UsbStorageDeviceInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetDeviceList(uint32_t* devices, int32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetMountPointOfShellCore(uint32_t device, char* mountPoint) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
bool APS5_VABI sceUsbStorageIsExist(uint32_t device, const char* directory, std::uint8_t* exists) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageRequestMap(uint32_t device, const char* directory, int32_t flags, uint64_t volumeSize, char* mountPoint, uint64_t* mountId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageRequestUnmap(uint32_t device, const char* directory) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetdentsOpen(uint32_t device, int32_t* fd, uint64_t* bufferSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetdentsRead(int32_t fd, void* buffer, uint64_t bufferSize, uint64_t* bytesRead) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceUsbStorageGetdentsClose(int32_t fd) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbStorageClearFakeWriteProtection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbStorageRequestMapWSB(uint32_t device_id, const char* subpath, int32_t flags,
                                        int32_t unknown, char* out_mount_path, int32_t* out_status) {
    NotImplemented_nid_no_patch(__func__);
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
