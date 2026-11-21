# libSceAjm.native

## add Functional

- [sceAjmDecWVorbisCreateHeaderPacket guest pointers](../../../core/libs/prx/libSceAjm.native/Export.cpp) (libSceAjmWVorbis, implemented in libSceAjm.native) - non-null unreadable or unwritable `param`, `setupPacket`, or `output` pointers return `0x8A6E0001` (`AJM_WVORBIS_ERROR_INVALID_PARAMETER`) with caller `errno` preserved via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` as host-safety fallbacks. Console behavior for invalid guest pointers is unmeasured.
