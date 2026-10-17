# libScePlayerInvitationDialog technical debt

### Functional

- [libScePlayerInvitationDialog](../../../core/libs/prx/libScePlayerInvitationDialog/Export.cpp) simulates dialog completion without displaying UI or sending invitations, so `scePlayerInvitationDialogGetResult` reports `USER_CANCELED`; its parameter ABI remains unverified. The 40-byte result layout (error code, result, 32 reserved bytes) is taken from [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/Dialog/PlayerInvitationDialog.cs) and the error codes from the other common dialogs, not confirmed on a title.
