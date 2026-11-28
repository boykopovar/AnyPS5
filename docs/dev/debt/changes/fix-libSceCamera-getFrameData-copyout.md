# libSceCamera

## add Functional

- [sceCameraGetFrameData](../../../core/libs/prx/libSceCamera/src/Handles.cpp) (libSceCamera) - returning SCE_CAMERA_ERROR_PARAM with caller errno preserved for an unwritable frame data pointer via GuestCopyOut_nid_no_patch is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
