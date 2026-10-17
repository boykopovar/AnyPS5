# libSceUsbStorage technical debt

### Silent stubs

- [libSceUsbStorage](../../../core/libs/prx/libSceUsbStorage/Export.cpp) - USB storage device management tracks subsystem initialization state via atomic lifecycle guards (`sceUsbStorageInit` and `sceUsbStorageTerm`). When no physical or virtual USB storage device is mounted, device enumeration (`sceUsbStorageGetDeviceList`) reports 0 connected devices, device queries (`sceUsbStorageGetDeviceInfo`, `sceUsbStorageGetdentsOpen`) return `SCE_USB_STORAGE_ERROR_NOENT`, and path mapping (`sceUsbStorageRequestMap`, `sceUsbStorageRequestMapWSB`) formats default mount destinations (`/mnt/usb0`). Callback registration stubs validate argument and event bounds without dispatching asynchronous hardware notifications.
