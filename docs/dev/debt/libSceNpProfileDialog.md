# libSceNpProfileDialog technical debt

### Silent stubs

- [sceNpProfileDialogOpen, sceNpProfileDialogOpenA, sceNpProfileDialogGetResult](../../../core/libs/prx/libSceNpProfileDialog/Export.cpp) (libSceNpProfileDialog) - no profile is shown: `Open` and `OpenA` move the dialog straight to `FINISHED`, and `GetResult` reports `result` 0, `userAction` `USER_CANCELED` (1) and the caller's `userData`, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/c2814041e546bf75c12756dbec9da911c721ce1c/src/core/libraries/np/np_profile_dialog/np_profile_dialog.cpp#L21-L180) does when its window is dismissed. Not measured on a console.

### Unknown function info

- [sceNpProfileDialogOpen, sceNpProfileDialogOpenA, sceNpProfileDialogGetResult](../../../core/libs/prx/libSceNpProfileDialog/Export.cpp) (libSceNpProfileDialog) - the layouts come from the [shadPS4 header](https://github.com/shadps4-emu/shadPS4/blob/c2814041e546bf75c12756dbec9da911c721ce1c/src/core/libraries/np/np_profile_dialog/np_profile_dialog.h#L31-L66) alone: `SceNpProfileDialogParam` 128 bytes with `userData` at 88, `SceNpProfileDialogParamA` 120 bytes with 4 unnamed bytes after `targetAccountId` and `userData` at 80, the result 48 bytes. A param whose `size` is not that size, or whose `mode` is not 1 to 4, throws. `baseParam` (size, magic), `userId` and the target ID are not checked.
- [sceNpProfileDialogOpen, sceNpProfileDialogOpenA, sceNpProfileDialogClose, sceNpProfileDialogGetResult](../../../core/libs/prx/libSceNpProfileDialog/Export.cpp) (libSceNpProfileDialog) - these paths throw because shadPS4 and the other common dialogs here return different errors: `Open` or `OpenA` before `Initialize` (shadPS4 `INVALID_STATE`, the Signin family `NOT_INITIALIZED`); `Close` on a dialog that is not running, which is every `Close` after `Initialize` here (shadPS4 `NOT_RUNNING`, the Signin family 0 and `FINISHED`); `GetResult` with a null result before the dialog finishes (shadPS4 `ARG_NULL`, Signin and NetCtlAp `NOT_FINISHED`).

### Functional

- [sceNpProfileDialogInitialize](../../../core/libs/prx/libSceNpProfileDialog/Export.cpp) (libSceNpProfileDialog) - never returns shadPS4's `NOT_SYSTEM_INITIALIZED` (`sceCommonDialogInitialize` not called) or `BUSY` (another common dialog in use): libSceCommonDialog shares neither state here.
