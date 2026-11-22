# libSceNpEulaDialog

## remove [sceNpEulaDialogCheckVersion and sceNpEulaDialogCheckVersionAbort]

## add Unknown function info

- [sceNpEulaDialogCheckVersion](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) (libSceNpEulaDialog) - `(int32_t userId, int32_t value, callback, void* userdata)` returns firmware 13.42's first checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21841#issuecomment-6939152850), disassembly only, not run on a console) and throws past them: `0x80B80003` before `sceNpEulaDialogInitialize`, then `0x8055A800` for `value <= 0` or a null callback. The console then checks for a running check, the title id and that `userId` is logged in, and starts a thread that reports the server's answer through the callback; none of that is implemented. `sceNpEulaDialogCheckVersionAbort(void)` returns `0x80B80003` before `sceNpEulaDialogInitialize` and otherwise `0x80B80006`, the console's answer when no check request exists, as none does here
