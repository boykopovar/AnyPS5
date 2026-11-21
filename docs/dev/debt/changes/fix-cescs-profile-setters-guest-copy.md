# libSceCesCs

## add Functional

- [sceCesIso2022JpUcsProfileSetSbcs, sceCesGbUcsProfileSet4ByteCharRange and sceCesGbUcsProfileSetUdaMapping guest pointers](../../../core/libs/prx/libSceCesCs/Export.cpp) (libSceCesCs) - sheet tags are read and setting bytes are written through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`; returning `CES_ERROR_INVALID_PROFILE` (`0x805C0004`) on unreadable or unwritable sheets is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
