# libScePlayerSelectionDialog technical debt

### Silent stubs

- [libScePlayerSelectionDialog, scePlayerSelectionDialogGetResult](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) shows no selection UI and immediately reports cancellation. Account discovery, selection and result delivery are unimplemented; returning `0x80B8000D` on unwritable result pointers is an unmeasured host-safety fallback. Tests cover lifecycle termination and Windows DLL dependencies with libSceUserService.
- [scePlayerSelectionDialogOpen](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - copies in and validates dialog parameters and optional player list safely; returning `0x80B8000D` on unreadable parameter or player array pointers is an unmeasured host-safety fallback.

### Unknown function info

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - zeroes parameters, sets common dialog base size and address-derived magic, and sets public size; returning `0x80B8000D` for unwritable parameter pointers is an unmeasured host-safety fallback. Console defaults for caller fields and reinitializing populated parameters remain unverified.
