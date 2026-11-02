# libScePlayerSelectionDialog technical debt

### Silent stubs

- [libScePlayerSelectionDialog, scePlayerSelectionDialogGetResult](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) shows no selection UI and immediately reports cancellation. Account discovery, selection and successful-result list delivery are not implemented; returning `0x80B8000D` for an unwritable result pointer is a host-safety fallback with console behavior unmeasured.

### Unknown function info

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - zeroes the parameter, sets the common dialog base size and address-derived magic, and sets the public size; returning `0x80B8000D` for an unwritable parameter pointer is a host-safety fallback with console behavior unmeasured. The console defaults for caller-owned fields and the exact behavior when reinitializing a populated parameter remain unverified.
