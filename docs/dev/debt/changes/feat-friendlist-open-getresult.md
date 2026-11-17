# libSceNpFriendListDialog

## change [sceNpFriendListDialogGetResult and sceNpFriendListDialogOpen]

- [sceNpFriendListDialogGetResult and sceNpFriendListDialogOpen](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp) (libSceNpFriendListDialog) - take the same arguments as the `A` variants and run their path here. In firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21842#issuecomment-6939202888), disassembly only, not run on a console) each is the same code as its `A` variant with the same checks in the same order; they differ only in the record layouts: `Open`'s mode-2 user array has 0x20-byte entries instead of 0x18, and `GetResult`'s record ends 0x10 bytes earlier, its mode-2 entries being 20 bytes instead of 32. Neither layout is read or written here beyond what the `A` variants do
