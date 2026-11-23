# libSceCamera

## add Unknown function info

- [sceCameraSetExposureGain, sceCameraSetWhiteBalance, sceCameraSetGamma](../../../core/libs/prx/libSceCamera/src/Attributes.cpp) (libSceCamera) - safely copy in parameter structures via GuestCopyIn_nid_no_patch with caller errno preserved; returning SCE_CAMERA_ERROR_PARAM for unmapped or unreadable pointers is a host-safety fallback with console behavior unmeasured.
