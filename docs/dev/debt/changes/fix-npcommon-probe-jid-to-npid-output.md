# libSceNpCommon

## add Unknown function info

- [sceNpUtilJidToNpId guest output](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the npid is built on the host and written with one GuestCopyOut_nid_no_patch; returning `0x80550601` (the null-pointer error) for an unwritable npid is a host-safety fallback with console behaviour unmeasured. The jid strings are still read in place.
