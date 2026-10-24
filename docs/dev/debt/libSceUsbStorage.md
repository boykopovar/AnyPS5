# libSceUsbStorage technical debt

### Unknown function info

- [sceUsbStorageClearFakeWriteProtection, sceUsbStorageSetFakeMapLockForDebug, sceUsbStorageSetFakeWriteProtection](../../../core/libs/prx/libSceUsbStorage/Export.cpp) (libSceUsbStorage) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceUsbStorage.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceUsbStorageRequestMapWSB](../../../core/libs/prx/libSceUsbStorage/Export.cpp) (libSceUsbStorage) - its prototype follows [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/usbd/usbstorage.cpp#L245-L246), not measured on a console; the throwing declaration only resolves imports.
