# libSceInvitationDialog technical debt

### Unknown function info

- [sceInvitationDialogClose](../../../core/libs/prx/libSceInvitationDialog/Export.cpp) (libSceInvitationDialog) - imported by PS4 titles, which do not load without it, and added without an implementation: declared without parameters, every call throws
