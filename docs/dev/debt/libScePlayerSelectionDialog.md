# libScePlayerSelectionDialog technical debt

### Functional

- [scePlayerSelectionDialogOpen](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) - returning `0x80B8000D` (`COMMON_DIALOG_ERROR_ARG_NULL`) for an unmapped or unreadable parameter pointer is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
- [scePlayerSelectionDialogGetResult, scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) - returning `0x80B8000D` on unwritable result or parameter pointers is an unmeasured host-safety fallback.

### Silent stubs

- [libScePlayerSelectionDialog](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) - shows no selection UI and immediately reports cancellation. Account discovery, selection and result delivery are unimplemented. Tests cover lifecycle termination and Windows DLL dependencies with libSceUserService.

### Unknown function info

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - zeroes parameters, sets common dialog base size and address-derived magic, and sets public size. Console defaults for caller fields and reinitializing populated parameters remain unverified.
