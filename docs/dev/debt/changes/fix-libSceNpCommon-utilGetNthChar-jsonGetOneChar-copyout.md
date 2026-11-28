# libSceNpCommon

## add Functional

- [sceNpUtilGetNthChar and sceNpUtilJsonGetOneChar guest outputs](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - character output pointer and JSON character outputs are written using GuestCopyOut_nid_no_patch to prevent invalid host memory dereferences; returning 0x80550601 on unwritable destination is host safety fallback with console behaviour unmeasured.
