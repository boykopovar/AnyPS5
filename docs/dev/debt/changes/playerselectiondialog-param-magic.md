# libScePlayerSelectionDialog

## add Functional

- [scePlayerSelectionDialogParamInitialize](../../../core/libs/prx/libScePlayerSelectionDialog/Export.cpp) (libScePlayerSelectionDialog) - calculating the common dialog base magic from `param` and `offsetof(PlayerSelectionDialogParam, base)` without dereferencing guest pointers, is a host-safety fallback; console fault behavior on invalid parameter buffers has not been measured.
