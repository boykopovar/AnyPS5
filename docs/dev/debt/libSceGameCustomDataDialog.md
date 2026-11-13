# libSceGameCustomDataDialog technical debt

### Silent stubs

- [libSceGameCustomDataDialog](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) - no dialog is shown and nothing is sent: an accepted `sceGameCustomDataDialogOpen` or `sceGameCustomDataDialogOpenA` finishes at once, as if the user cancelled, so the status never becomes `RUNNING`, `sceGameCustomDataDialogClose` can only answer `COMMON_DIALOG_ERROR_NOT_RUNNING`, and `GetResult` reports `COMMON_DIALOG_RESULT_USER_CANCELED` with an empty sent-users list.

### Functional

- [Unreadable or unwritable guest memory](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) - a param, data param, string, platform list, recipient list, result or sent-users list that can't be read or written makes `Open`, `OpenA`, `GetResult` and `GetResultA` return `COMMON_DIALOG_ERROR_ARG_NULL`. That is a host-safety fallback; [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/b97bafa353d8/src/core/libraries/game_custom_data_dialog/game_custom_data_dialog.cpp#L98-L255) dereferences them, and the console's behaviour is unmeasured.
- [The signed-in check](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) - `Open` and `OpenA` read the signed-in users through `sceUserServiceGetLoginUserIdList`, so before `sceUserServiceInitialize` every user counts as signed out and the param is rejected with `COMMON_DIALOG_ERROR_INVALID_ARGUMENT`.
- [sceGameCustomDataDialogInitialize](../../../core/libs/prx/libSceGameCustomDataDialog/Export.cpp) - doesn't check that the common dialog system was initialized or that another common dialog is in use, which [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/b97bafa353d8/src/core/libraries/game_custom_data_dialog/game_custom_data_dialog.cpp#L612-L629) answers with `COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED` and `COMMON_DIALOG_ERROR_BUSY`.
