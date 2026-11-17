# libSceCamera

## add Functional

- [sceCameraStart and sceCameraStartByHandle](../../../core/libs/prx/libSceCamera/src/Handles.cpp) (libSceCamera) - returning `SCE_CAMERA_ERROR_PARAM` for an unmapped non-null start parameter via a checked guest read is a host-safety fallback; console fault behavior on unmapped guest memory is unmeasured.
