#ifndef CORE_LIBS_PRX_LIBSCEUSBSTORAGE_USBSTORAGE_HPP
#define CORE_LIBS_PRX_LIBSCEUSBSTORAGE_USBSTORAGE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace UsbStorage {

inline constexpr std::int32_t InvalidArgument = static_cast<std::int32_t>(0x80f40002);
inline constexpr std::int32_t DeviceNotFound = static_cast<std::int32_t>(0x80f40004);
inline constexpr std::int32_t NotSupported = static_cast<std::int32_t>(0x80f40005);

inline constexpr std::int32_t CallbackEventTypeFirst = 1;
inline constexpr std::int32_t CallbackEventTypeLast = 2;

}

struct UsbStorageEvent;
struct UsbStorageDeviceInfo;
using UsbStorageCallback = void (APS5_VABI *)(const UsbStorageEvent* event, void* userData);

extern "C" {
std::int32_t APS5_VABI sceUsbStorageInit(void* attr);
std::int32_t APS5_VABI sceUsbStorageTerm(void);
std::int32_t APS5_VABI sceUsbStorageRegisterCallback(std::int32_t eventType, UsbStorageCallback callback, void* arg);
std::int32_t APS5_VABI sceUsbStorageUnregisterCallback(std::int32_t eventType, UsbStorageCallback callback);
std::int32_t APS5_VABI sceUsbStorageRegisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg);
std::int32_t APS5_VABI sceUsbStorageUnregisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback);
std::int32_t APS5_VABI sceUsbStorageRegisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg);
std::int32_t APS5_VABI sceUsbStorageUnregisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback);
std::int32_t APS5_VABI sceUsbStorageGetDeviceInfo(std::uint32_t device, UsbStorageDeviceInfo* info);
std::int32_t APS5_VABI sceUsbStorageGetDeviceList(std::uint32_t* devices, std::int32_t* count);
std::int32_t APS5_VABI sceUsbStorageGetMountPointOfShellCore(std::uint32_t device, char* mountPoint);
bool APS5_VABI sceUsbStorageIsExist(std::uint32_t device, const char* directory, std::uint8_t* exists);
std::int32_t APS5_VABI sceUsbStorageRequestMap(std::uint32_t device, const char* directory, std::int32_t flags,
                                               std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId);
std::int32_t APS5_VABI sceUsbStorageRequestUnmap(std::uint32_t device, const char* directory);
std::int32_t APS5_VABI sceUsbStorageGetdentsOpen(std::uint32_t device, std::int32_t* fd, std::uint64_t* bufferSize);
std::int32_t APS5_VABI sceUsbStorageGetdentsRead(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead);
std::int32_t APS5_VABI sceUsbStorageGetdentsClose(std::int32_t fd);
std::int32_t APS5_VABI sceUsbStorageRequestMapWSB(std::uint32_t device, const char* subpath, std::int32_t flags,
                                                  std::int32_t unknown, char* mountPath, std::int32_t* status);
std::int32_t APS5_VABI sceUsbStorageClearFakeWriteProtection();
std::int32_t APS5_VABI sceUsbStorageSetFakeMapLockForDebug();
std::int32_t APS5_VABI sceUsbStorageSetFakeWriteProtection();
}

#endif
