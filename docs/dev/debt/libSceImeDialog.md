# libSceImeDialog technical debt

### Silent stubs

- [sceImeDialogInit](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) shows no dialog: the text is typed on the host keyboard, which [ImeTextInput](../../../core/libs/prx/libSceVideoOut/src/ImeTextInput.cpp) forwards from the game window through libSceIme while no IME panel is open. Text, Backspace, Delete, Left, Right, Home and End edit the text, Enter accepts it (end status 0) and Escape cancels (end status 1). The controller cannot type, the title, placeholder, type and options are not shown or applied, and line breaks cannot be typed. While the dialog runs, key presses do not reach the keyboard-to-pad bindings of [PadInput](../../../core/libs/prx/libSceVideoOut/src/PadInput.cpp)

### Unknown function info

- [sceImeDialogInit, sceImeDialogGetStatus, sceImeDialogGetResult, sceImeDialogAbort, sceImeDialogTerm](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) - the statuses (none 0, running 1, finished 2), the end statuses (ok 0, user canceled 1, aborted 2) and the errors of `sceImeDialogInit` for an open dialog (`0x80bc0001`), a null parameter (`0x80bc0031`), a maximum text length outside 1 to 2048 (`0x80bc0016`) and a null text buffer (`0x80bc0017`) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/b2d7ec7d31f9ad9426db93afe079db8679b3e1e0/src/core/libraries/ime/ime_dialog.cpp); none is measured on a PS5. A result, abort or term on a dialog that is not finished, running or open returns `0x80bc0002`, where shadPS4 returns `0x80bc0105` to `0x80bc0107`, and `sceImeDialogTerm` also closes a running dialog, which shadPS4 refuses. The text reaches the title's buffer only when Enter accepts it
