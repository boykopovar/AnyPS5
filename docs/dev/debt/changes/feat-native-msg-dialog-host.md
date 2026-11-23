# libSceErrorDialog

## add Unknown function info

- [the error host box](../../../core/libs/prx/libSceErrorDialog/Export.cpp) (libSceErrorDialog) - only with `ANYPS5_ERROR_DIALOG_HOST=1` and not on Apple builds. The box is shown with `SDL_ShowMessageBox` while the call holds the dialog lock, so the call and the status calls wait until a button is pressed. The text is the error dialog's message reduced to ASCII. The box is the one of libSceMsgDialog.

# libSceMsgDialog.native

## add Unknown function info

- [libSceMsgDialog.native](../../../core/libs/prx/libSceMsgDialog.native/Export.cpp) - with `ANYPS5_MSG_DIALOG_HOST=1` (not on Apple builds) a user message is answered by the host message box of libSceMsgDialog, as in libSceMsgDialog: the call waits until a button is pressed, then reports it. The gamepad does not answer while the box is open. If the box cannot be shown, the in-game dialog is used.

# libSceMsgDialog

## remove is kept but not returned
