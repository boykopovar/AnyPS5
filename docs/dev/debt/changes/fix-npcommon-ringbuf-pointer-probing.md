# libSceNpCommon

## add Unknown function info

- [sce::np::RingBuf D1, ctor, and Init](../../../core/libs/prx/libSceNpCommon/RingBuf.cpp) (libSceNpCommon) - probe and copy the `NpRingBuf` object via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` with caller `errno` preserved; returning early from `D1`/`ctor` or returning `NP_ERROR_INVALID_ARGUMENT` from `Init` on null or unmapped `self` pointers is a host-safety fallback with console behavior unmeasured.
