# libSceNpCommon

## add Unknown function info

- [sceNpUtilJsonEscape destination](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the text already in the destination is read with `CopyGuestString_nid_no_patch` and the escaped text is appended with one `GuestCopyOut_nid_no_patch`; an unreadable or unwritable destination returns `SCE_NP_UTIL_ERROR_INVALID_ARGUMENT` like a null one, because the console's behaviour on unmapped memory is unmeasured. The source is still read in place.
