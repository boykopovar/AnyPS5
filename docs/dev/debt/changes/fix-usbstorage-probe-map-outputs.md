# libSceUsbStorage

## add Unknown function info

- [map outputs](../../../core/libs/prx/libSceUsbStorage/Export.cpp) (libSceUsbStorage) - `sceUsbStorageGetMountPointOfShellCore`, `sceUsbStorageRequestMap` and `sceUsbStorageRequestMapWSB` write the mount point, the map id and the WSB status with `GuestCopyOut_nid_no_patch`; an unwritable output returns `SCE_USBSTORAGE_ERROR_INVALID_ARGUMENT` like the other unwritable outputs of the library, and a map request removes the map it just added and raises no map-available callback, because the console's behaviour on unmapped memory is unmeasured.
