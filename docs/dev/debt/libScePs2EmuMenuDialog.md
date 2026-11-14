# libScePs2EmuMenuDialog technical debt

### Unknown function info

- [scePs2EmuMenuDialogClose, scePs2EmuMenuDialogGetResult, scePs2EmuMenuDialogGetStatus, scePs2EmuMenuDialogInitialize, scePs2EmuMenuDialogOpen, scePs2EmuMenuDialogTerminate and scePs2EmuMenuDialogUpdateStatus](../../../core/libs/prx/libScePs2EmuMenuDialog/Export.cpp) (libScePs2EmuMenuDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePs2EmuMenuDialog.c). Behaviour follows a measurement on firmware 13.42 reported in PR #9634: `Initialize`, `Open`, `GetResult`, `Close` and `Terminate` always return `0x80B8000F` and `UpdateStatus`/`GetStatus` return 0 (`COMMON_DIALOG_STATUS_NONE`), with no state and no writes to the argument, checked with null, small and all-ones arguments. Other firmware versions and the parameter types remain unmeasured; the declarations have no parameters and this is not an ABI claim.
