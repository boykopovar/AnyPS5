# libSceCdlgG2P technical debt

### Silent stubs

- [libSceCdlgG2P](../../../core/libs/prx/libSceCdlgG2P/Export.cpp) - the G2P dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled. The 128-byte open parameters, common-header validation and result field at byte 8 are recovered. Native service state transitions remain unimplemented; PPSA01289 imports it; returning `COMMON_DIALOG_ERROR_INVALID_ARGUMENT` for an unwritable result is a host-safety fallback as console behavior for unwritable memory has not been measured
