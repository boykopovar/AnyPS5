# libSceSigninDialog technical debt

### Silent stubs

- [libSceSigninDialog](../../../core/libs/prx/libSceSigninDialog/Export.cpp) shows no sign-in UI and immediately transitions to finished status with a user-cancelled result.
- [sceSigninDialogOpen, sceSigninDialogClose](../../../core/libs/prx/libSceSigninDialog/Export.cpp) (libSceSigninDialog) - calling `sceSigninDialogOpen` on an already finished dialog succeeds with status remaining `FINISHED`, and calling `sceSigninDialogClose` while finished returns 0; this finished-state reopen and close lifecycle is emulator stub behavior and is unmeasured on console hardware.

### Unknown function info

- [sceSigninDialogClose, sceSigninDialogGetResult](../../../core/libs/prx/libSceSigninDialog/Export.cpp) (libSceSigninDialog) - SceSigninDialogResult layout (int32 result and 3 reserved int32 words, zeroed) follows [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/main/src/libs/dialog.cpp) and [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/SigninDialog/SigninDialog.cs); error codes are the 0x80B8xxxx common-dialog codes rather than KytyPS5's 0x8135xxxx, and offline calls finish as user cancelled without opening a dialog. Neither choice is verified on hardware. The callers agree with the 16-byte size and the result at offset 0: before their one `sceSigninDialogGetResult` call, PPSA01494, PPSA30839 and PPSA31162 zero a 16-byte result on the stack, and right after it compare its first 32-bit word with 1
