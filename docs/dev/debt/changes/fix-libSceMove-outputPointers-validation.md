# libSceMove

## add Functional

- [sceMoveGetBluetoothAddress, sceMoveGetDeviceId and sceMoveReadStateRecent guest pointers](../../../core/libs/prx/libSceMove/Export.cpp) (libSceMove) - destination address, device ID and recent read count pointers are validated with GuestWritable_nid_no_patch; returning MOVE_ERROR_INVALID_ARG on unwritable destination is host safety fallback with console behaviour unmeasured.
