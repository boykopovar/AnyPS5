# libSceCdlgG2P technical debt

### Functional

- [sceG2PDialogOpen](../../../core/libs/prx/libSceCdlgG2P/Export.cpp) - returning `0x80B8000D` (`COMMON_DIALOG_ERROR_ARG_NULL`) for an unmapped or unreadable parameter pointer is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
- [sceG2PDialogGetResult](../../../core/libs/prx/libSceCdlgG2P/Export.cpp) - returning `COMMON_DIALOG_ERROR_INVALID_ARGUMENT` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Silent stubs

- [libSceCdlgG2P](../../../core/libs/prx/libSceCdlgG2P/Export.cpp) - the G2P dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled. The 128-byte open parameters, common-header validation and result field at byte 8 are recovered. Native service state transitions remain unimplemented; PPSA01289 imports it.
