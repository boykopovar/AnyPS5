# libScePlayerSelectionDialog technical debt

### Silent stubs

- [libScePlayerSelectionDialog](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) shows no selection UI and immediately reports cancellation. Account discovery, selection and successful-result list delivery are not implemented.

### Unknown function info

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - returns `COMMON_DIALOG_ERROR_ARG_NULL` on a null parameter and throws `NotImplemented_nid_no_patch` for non-null pointers because `ScePlayerSelectionDialogParam` struct layout and default fields remain unverified.
