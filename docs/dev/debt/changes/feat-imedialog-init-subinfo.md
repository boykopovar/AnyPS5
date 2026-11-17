# libSceImeDialog

## remove The following pending declarations have unverified signatures and behavior

## add Unknown function info

- [sceImeDialogInitInternalWithSubInfo](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) - takes `(param, ext, const void* subInfo)` and checks them as firmware 13.42's export does ([reading](https://github.com/boykopovar/AnyPS5/issues/21837#issuecomment-6938945829), disassembly only, not run on a console): an open dialog returns `ErrorBusy`, then the parameter checks `sceImeDialogInit` shares (a null `param` gives `ErrorInvalidAddress`), then it reads the 32-bit `subInfo+0x10`. A null or unreadable `subInfo` throws here, since the console faults there. Past that it opens the dialog as `sceImeDialogInit` does. 13.42 also copies the whole 0x128-byte sub-info into the dialog and passes its `+0x10` to the request builder; what either changes wasn't read, so the sub-info has no effect here. Closes when: the request builder (`0x20b0`, `0x23a0`) is read.
