# libSceDiscId

## add Functional

- [sceDiscIdGet, sceDiscIdGet2](../../../core/libs/prx/libSceDiscId/Export.cpp) (libSceDiscId) - returning `ErrorInvalidArgument` when discId output pointer is unmapped via GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch is a host-safety fallback preserving host errno; console behavior on unmapped memory is unmeasured.
