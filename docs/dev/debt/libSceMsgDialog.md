# libSceMsgDialog technical debt

### Silent stubs

- [libSceMsgDialog](../../../core/libs/prx/libSceMsgDialog/Export.cpp) - message dialogs show no UI. `sceMsgDialogOpen` opens one without `sceMsgDialogInitialize` as well, and a user message or system message dialog finishes on the second `sceMsgDialogGetStatus` or `sceMsgDialogUpdateStatus` call; the first 160 characters of a user message are only logged. The button it picks (2 for `YES_NO_FOCUS_NO`, otherwise 1) is kept but not returned, as `sceMsgDialogGetResult` is exported by libSceMsgDialog.native. A progress bar dialog runs until `sceMsgDialogClose` or for 30 seconds, and `sceMsgDialogProgressBarInc`, `sceMsgDialogProgressBarSetMsg` and `sceMsgDialogProgressBarSetValue` ignore their arguments

### Unknown function info

- [sceMsgDialogOpen](../../../core/libs/prx/libSceMsgDialog/Export.cpp) (libSceMsgDialog) - the `SceMsgDialogParam` layout it reads is not measured on a PS5: the mode at 0x38 (1 user message, 2 progress bar, 3 system message), the user message parameter pointer at 0x40, and in that parameter the button type at 0 and the message pointer at 8. A mode outside 1 to 3, or a parameter that cannot be read, is treated as a user message; only Windows checks that the memory is readable, so on Linux any non-null pointer is read. The errors `0x80b80003` (not initialized), `0x80b80007` (busy), `0x80b8000b` (not running) and `0x80b8000d` (null argument) are not measured either. Closes when: measured on a PS5
