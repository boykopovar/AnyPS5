# libSceNpFriendListDialog technical debt

### Silent stubs

- [sceNpFriendListDialogOpenA](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp) (libSceNpFriendListDialog) - with `ANYPS5_PSN_DIALOGS=show`, a successful open shows AnyPS5's notice that the feature needs PlayStation Network; the console's own display without PlayStation Network is not measured, and the notice wording is AnyPS5's.
- [libSceNpFriendListDialog](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp) - the friend list dialog (`sceNpFriendListDialogOpenA` and `sceNpFriendListDialogGetResultA`) runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no PSN friend list. The signatures are from [Orbital's type database](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json); the result layout beyond the leading `int32_t` result code is not known, so nothing else is written.

### Unknown function info

- [sceNpFriendListDialogGetResult and sceNpFriendListDialogOpen](../../../core/libs/prx/libSceNpFriendListDialog/Export.cpp) (libSceNpFriendListDialog) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpFriendListDialog.c), but Orbital has prototypes only for the `A` variants and no credible prototypes were found for these. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
