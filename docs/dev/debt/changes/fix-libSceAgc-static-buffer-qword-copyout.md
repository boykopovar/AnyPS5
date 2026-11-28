# libSceAgc

## add Functional

- [sceAgcGetStaticQword](../../../core/libs/prx/libSceAgc/Misc/src/StaticBuffer.cpp) (libSceAgc) - value output pointer is validated and written through GuestCopyOut_nid_no_patch with caller errno preserved; returning GRAPHICS5_ERROR_INVALID_STATIC_BUFFER on unwritable pointer is host safety fallback with console behavior unmeasured.
