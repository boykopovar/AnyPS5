# libSceVideoOut

## add Unknown function info

- [sceVideoOutAdjustColorSpaceConversion](../../../core/libs/prx/libSceVideoOut/src/Output.cpp) (libSceVideoOut) - probes guest conversion settings buffer via GuestCopyIn_nid_no_patch with caller errno preserved; throwing VIDEO_OUT_ERROR_INVALID_ADDRESS for an unmapped pointer is a host-safety fallback with console behavior unmeasured.
