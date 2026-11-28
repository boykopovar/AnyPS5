# libSceVideoOut

## add Functional

- [sceVideoOutGetEventId](../../../core/libs/prx/libSceVideoOut/src/Event.cpp) (libSceVideoOut) - returning VIDEO_OUT_ERROR_INVALID_ADDRESS for unmapped event pointers via GuestCopyIn_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
