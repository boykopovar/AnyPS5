# libScePlayerSelectionDialog technical debt

### Silent stubs

- [libScePlayerSelectionDialog](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) shows no selection UI and immediately reports cancellation. Account discovery, selection and successful-result list delivery are not implemented.

### Unknown function info

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - zeroes the parameter, sets the common dialog base size and address-derived magic, and sets the public size. The console defaults for caller-owned fields and the exact behavior when reinitializing a populated parameter remain unverified.
