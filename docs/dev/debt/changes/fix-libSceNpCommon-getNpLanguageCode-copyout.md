# libSceNpCommon

## add Unknown function info

- [sceNpUtilGetNpLanguageCode](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - copies out language code via GuestCopyOut_nid_no_patch with caller errno preserved; returning NP_ERROR_INVALID_ARGUMENT_UTIL for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
