# libSceHttp

## change sceHttpGetAutoRedirect, sceHttpGetNonblock

- [sceHttpGetAutoRedirect, sceHttpGetNonblock](../../../core/libs/prx/libSceHttp/src/Options.cpp) (libSceHttp) - returning ERROR_INVALID_VALUE for unwritable enable pointers via GuestCopyOut_nid_no_patch with caller errno preserved is a host-safety fallback; console behavior on unmapped memory is unmeasured.
