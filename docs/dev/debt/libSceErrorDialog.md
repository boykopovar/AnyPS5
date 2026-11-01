# libSceErrorDialog technical debt

### Silent stubs

- [libSceErrorDialog](../../../core/libs/prx/libSceErrorDialog/Export.cpp) - the error dialog shows UI only while the video output has a window to draw it and no other dialog is shown and a flip draws it within three seconds; otherwise it runs the state machine, finishes on the first `sceErrorDialogUpdateStatus` call and the error code passed to `sceErrorDialogOpen` is only logged

### Unknown function info

- [sceErrorDialogOpenDetail, sceErrorDialogOpenWithReport](../../../core/libs/prx/libSceErrorDialog/Export.cpp) (libSceErrorDialog) - their names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceErrorDialog.c); the no-parameter prototypes follow [shadPS4 `error_dialog.cpp`](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/ime/error_dialog.cpp#L167-L172) and are not measured on a console. They are declared without parameters solely to resolve known imports. This is not an ABI claim. Both currently return `kErrUnexpectedFatal` (`0x80ED0004`); this return behavior is covered by unit tests but has not been independently verified on console hardware.
- [sceErrorDialogOpen](../../../core/libs/prx/libSceErrorDialog/include/ErrorDialogRequest.hpp) (libSceErrorDialog) - the overlay dialog is not modelled on a PS5: the wording `An error occurred.` and `Error code:` and the label OK are the host's own and English, the code is shown as eight hex digits, and no text is looked up for the code. The 16-byte parameter is not measured beyond the size, error code and user id the function already reads. Closes when: measured on a PS5
