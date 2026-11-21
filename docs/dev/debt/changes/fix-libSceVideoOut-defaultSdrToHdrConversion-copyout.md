# libSceVideoOut

## add Unknown function info

- [sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion](../../../core/libs/prx/libSceVideoOut/src/Output.cpp) (libSceVideoOut) - zero-initializes conversion settings via GuestCopyOut_nid_no_patch with caller errno preserved; throwing VIDEO_OUT_ERROR_INVALID_ADDRESS for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
