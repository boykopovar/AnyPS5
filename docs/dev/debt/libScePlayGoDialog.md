# libScePlayGoDialog technical debt

### Silent stubs

- [scePlayGoDialogGetResult](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) - returning `0x80B8000D` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.
- [scePlayGoDialogGetStatus and scePlayGoDialogUpdateStatus](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) return dialog state values without updating host dialog presentation or download status.

### Unknown function info

- [scePlayGoDialogOpen](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) does not validate or copy the native nested parameters or query download progress. The host UI stub immediately reports completion; service-driven failures and transitions remain unimplemented.
