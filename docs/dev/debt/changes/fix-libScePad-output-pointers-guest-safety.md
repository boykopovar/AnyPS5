# libScePad

## add Functional

- [scePadGetMotionSensorPosition, scePadGetSphereRadius and scePadGetBluetoothAddress guest pointers](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - destination position, radius and bluetooth address pointers are validated with GuestWritable_nid_no_patch; returning PAD_ERROR_INVALID_ARG on unwritable destination is host safety fallback with console behaviour unmeasured.
