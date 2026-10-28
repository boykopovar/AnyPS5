# libScePlayerInvitationDialog technical debt

### Silent stubs

- [scePlayerInvitationDialogTerminate and scePlayerInvitationDialogUpdateStatus](../../../core/libs/prx/libScePlayerInvitationDialog/Export.cpp) (libScePlayerInvitationDialog) - dialog termination and status update return 0 without displaying UI, matching the simulated dialog completion model so titles proceed without blocking on user interaction.

### Functional

- [libScePlayerInvitationDialog](../../../core/libs/prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.cpp) simulates dialog completion without displaying UI or sending invitations, so `scePlayerInvitationDialogGetResult` reports `USER_CANCELED`; its parameter ABI remains unverified. The 40-byte result layout (error code, result, 32 reserved bytes) is taken from [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/Dialog/PlayerInvitationDialog.cs) and the error codes from the other common dialogs, not confirmed on a title.
