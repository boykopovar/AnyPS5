# libSceNpCommon

## add Unknown function info

- [sceNpStrcpyToBuf and sceNpStrncpyToBuf](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the source is copied in with `CopyGuestString_nid_no_patch` bounded by the buffer size (and the length limit), which is all the fit check needs, and the padded buffer is written with one `GuestCopyOut_nid_no_patch`; an unreadable source returns `SCE_NP_UTIL_ERROR_INVALID_ARGUMENT` like a null one and an unwritable buffer returns `SCE_NP_ERROR_INVALID_ARGUMENT`, because the console's behaviour on unmapped memory is unmeasured.
