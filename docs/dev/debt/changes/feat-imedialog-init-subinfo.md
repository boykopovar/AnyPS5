# libSceImeDialog

## remove The following pending declarations have unverified signatures and behavior

## add Unknown function info

- [sceImeDialogInitInternalWithSubInfo](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) - takes `(param, ext, const void* subInfo)` ([13.42 reading](https://github.com/boykopovar/AnyPS5/issues/21837#issuecomment-6938945829), disassembly only). An open dialog returns `ErrorBusy`; then the parameter check it shares with `sceImeDialogInitInternal`, where a null `param` gives `ErrorInvalidAddress` but the option-mask check (`0x80bc0015`) isn't modelled, as in `InitInternal` here; then it reads the 32-bit `subInfo+0x10`, throwing for a null or unreadable `subInfo` where the console faults. `main`'s `0x80bc0016`/`0x80bc0017` checks run after that read; the reading doesn't say which routine makes them. Past that it opens the dialog as `sceImeDialogInit` does: 13.42 copies the 0x128-byte sub-info into the dialog and passes its `+0x10` to the request builder (`0x20b0`, `0x23a0`), which wasn't read, so the sub-info has no effect here. Closes when that builder is read.
