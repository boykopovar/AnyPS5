# libSceErrorDialog technical debt

### Silent stubs

- [libSceErrorDialog](../../../core/libs/prx/libSceErrorDialog/Export.cpp) - the error dialog runs the state machine but shows nothing; the error code passed to `sceErrorDialogOpen` is only logged

### Unknown function info

- [sceErrorDialogOpenDetail, sceErrorDialogOpenWithReport](../../../core/libs/prx/libSceErrorDialog/Export.cpp) (libSceErrorDialog) - their names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceErrorDialog.c); the no-parameter prototypes follow [shadPS4 `error_dialog.cpp`](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/ime/error_dialog.cpp#L167-L172) and are not measured on a console. They are declared without parameters solely to resolve known imports; this is not an ABI claim, and every call throws.
