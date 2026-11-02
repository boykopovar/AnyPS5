# libSceNpSnsYouTubeDialog technical debt

### Unknown function info

- [sceNpSnsYouTubeDialogOpen, sceNpSnsYouTubeDialogGetResult](../../../core/libs/prx/libSceNpSnsYouTubeDialog/Export.cpp) (libSceNpSnsYouTubeDialog) - names and NIDs appear in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpSnsYouTubeDialog.c). Parameterless declarations resolve known imports without ABI claims; initialized calls throw through `NotImplemented_nid_no_patch`.

### Functional

- [sceNpSnsYouTubeDialogOpen, sceNpSnsYouTubeDialogClose, sceNpSnsYouTubeDialogGetResult](../../../core/libs/prx/libSceNpSnsYouTubeDialog/Export.cpp) (libSceNpSnsYouTubeDialog) - return `0x80B80003` (`COMMON_DIALOG_ERROR_NOT_INITIALIZED`) when uninitialized before initialization and after termination, inferred from the common dialog lifecycle model rather than console traces.
