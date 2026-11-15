# libSceSystemService

## add Functional

- [sceSystemServiceInitializePlayerDialogParam](../../../core/libs/prx/libSceSystemService/Export.cpp) - the 0x40-byte initialized descriptor is written through `GuestCopyOut_nid_no_patch` rather than direct guest dereferences. On an unmapped non-null output the host throws the same `std::invalid_argument` path used for null and preserves host `errno`. The native console behavior for an unmapped non-null pointer is not measured; the observed null-pointer console fault remains documented above.
