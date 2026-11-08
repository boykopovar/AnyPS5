# libScePlayGoDialog technical debt

### Functional

- [scePlayGoDialogGetResult](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) (libScePlayGoDialog) - returning ERROR_ARG_NULL (0x80B8000D) for unmapped or unwritable result pointers is a host-safety fallback; console fault behavior on unmapped memory is unmeasured.

### Silent stubs

- [scePlayGoDialogGetStatus and scePlayGoDialogUpdateStatus](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) return dialog state values without updating host dialog presentation or download status.

### Unknown function info

- [scePlayGoDialogOpen](../../../core/libs/prx/libScePlayGoDialog/Export.cpp) does not validate or copy the native nested parameters or query download progress. The host UI stub immediately reports completion; service-driven failures and transitions remain unimplemented.
