# libScePlayerSelectionDialog technical debt

### Unknown function info

- [scePlayerSelectionDialogClose, scePlayerSelectionDialogGetResult, scePlayerSelectionDialogGetStatus, scePlayerSelectionDialogInitialize, scePlayerSelectionDialogOpen, scePlayerSelectionDialogParamInitialize, scePlayerSelectionDialogTerminate, scePlayerSelectionDialogUpdateStatus](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - Common dialog lifecycle is implemented as an immediate finish reporting USER_CANCELED; error codes follow libSceLoginDialog 0x80B8xxxx conventions. Parameter ABI is unverified on hardware
