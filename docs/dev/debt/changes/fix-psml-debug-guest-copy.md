# libScePsml_debug

## add Functional

- [MFSR guest pointer validation](../../../core/libs/prx/libScePsml_debug/Export.cpp) (libScePsml_debug) - returning `SCE_PSML_ERROR_INVALID_ARGUMENT` from `scePsmlMfsrGetDispatchMfsrPacketSizeInDwords` and `scePsmlMfsrGetMipmapBias` or throwing on unwritable/unreadable guest pointers in `scePsmlMfsrGetSharedResourcesInitRequirement`, `scePsmlMfsrCreateSharedResources`, `scePsmlMfsrCreateContext`, `ContextBufferRequirement` and `CreateContextFromParam` via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` is a host-safety fallback; console behaviour on invalid non-null pointers is unmeasured.
