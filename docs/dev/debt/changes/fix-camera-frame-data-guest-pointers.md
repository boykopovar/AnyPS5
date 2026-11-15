# libSceCamera

## add Functional

- [sceCameraGetFrameData guest pointers](../../../core/libs/prx/libSceCamera/src/Handles.cpp) (libSceCamera) - returning SCE_CAMERA_ERROR_PARAM for an unreadable or unwritable frame data pointer is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
