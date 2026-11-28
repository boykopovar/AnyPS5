# libSceCamera

## add Unknown function info

- [sceCameraSetAutoExposureGain](../../../core/libs/prx/libSceCamera/src/Attributes.cpp) (libSceCamera) - copies in auto exposure gain target structure via GuestCopyIn_nid_no_patch with caller errno preserved; returning SCE_CAMERA_ERROR_PARAM for an unmapped or unreadable pointer is a host-safety fallback with console behavior unmeasured.
