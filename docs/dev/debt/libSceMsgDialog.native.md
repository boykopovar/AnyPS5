# libSceMsgDialog.native technical debt

### Silent stubs

- [libSceMsgDialog.native](../../../core/libs/prx/libSceMsgDialog.native/Export.cpp) - only the user message dialogs that [libSceMsgDialog](libSceMsgDialog.md) lists as shown wait for the player; every other dialog finishes at `sceMsgDialogOpen` without UI and answers with button 1, so `sceMsgDialogClose` returns `NOT_RUNNING` for it, and a shown one that no flip drew within three seconds finishes at the next status call with button 1; its result before `sceMsgDialogInitialize` follows libSceMsgDialog. `sceMsgDialogTerminate` resets running or finished dialogs and returns OK, rejecting uninitialized calls with `COMMON_DIALOG_ERROR_NOT_INITIALIZED` following shadPS4; null pointer arguments return `COMMON_DIALOG_ERROR_ARG_NULL`.

### Unknown function info

- [sceMsgDialogGetResult](../../../core/libs/prx/libSceMsgDialog.native/Export.cpp) (libSceMsgDialog.native) returns the button id the player picked in a shown dialog, and 1 for every other dialog and for one the title closed; the result code is always 0. Which button ids and result codes a PS5 reports after a close or a cancel is not measured. `sceMsgDialogOpen` closes a dialog that is still waiting and shows the new one, instead of failing as libSceMsgDialog does with busy. Closes when: measured on a PS5
