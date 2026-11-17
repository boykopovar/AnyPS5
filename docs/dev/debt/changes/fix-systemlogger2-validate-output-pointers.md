# libSceSystemLogger2

## add Functional

- [sceSystemLogger2GetTimestamp, sceSystemLogger2GetEventStatus, sceSystemLogger2SetEventAsyncInternal](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) - output pointers and guest string inputs are validated upfront with `GuestWritable_nid_no_patch` and `CopyGuestString_nid_no_patch` before copying outputs via `GuestCopyOut_nid_no_patch`, preserving caller `errno` across failures. Native console behavior when encountering unmapped output descriptors or invalid string parameters is safely mapped to `INVALID_ARGUMENT`.
