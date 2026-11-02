# libSceNpEulaDialog technical debt

### Unknown function info

- [sceNpEulaDialog exports](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) (libSceNpEulaDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpEulaDialog.c); no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim, and initialized calls throw through `NotImplemented_nid_no_patch`.

### Functional

- [libSceNpEulaDialog](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) - tracks lifecycle across `Initialize`, `Terminate`, `GetStatus`, `UpdateStatus`. `Open`, `Close`, `GetResult`, `CheckVersion`, `CheckVersionAbort` return `COMMON_DIALOG_ERROR_NOT_INITIALIZED` (`0x80B80003`) before initialization and after termination, inferred from common dialog lifecycle model. Not measured on a console.
