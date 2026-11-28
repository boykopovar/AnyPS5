# libSceUsbStorage

## add Functional

- [sceUsbStorageUnregisterCallback, map-callback unregistration, and 13.60 pre-init checks](../../../core/libs/prx/libSceUsbStorage/Export.cpp) - Disassembly of firmware 13.60 `libSceUsbStorage.sprx` confirms `sceUsbStorageUnregisterCallback` returns `0x80f40009` when uninitialized and `0x80f40002` when unregistered, `sceUsbStorageUnregisterCallbackForMapAvailable` and `sceUsbStorageUnregisterCallbackForMapUnavailable` return `0x80f40002` when unregistered, and `sceUsbStorageRequestUnmap`, `sceUsbStorageGetdentsClose`, the fake write-protection/map-lock stubs, `sceUsbStorageCheckSystemDefaultMountPoint`, and the four zero-arg M2/terminate client stubs return `0x80f40009` when uninitialized.
