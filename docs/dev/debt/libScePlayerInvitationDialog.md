# libScePlayerInvitationDialog technical debt

### Silent stubs

- [scePlayerInvitationDialogTerminate and scePlayerInvitationDialogUpdateStatus](../../../core/libs/prx/libScePlayerInvitationDialog/Export.cpp) (libScePlayerInvitationDialog) - dialog termination and status update return 0 without displaying UI, matching the simulated dialog completion model so titles proceed without blocking on user interaction.

### Functional

- [libScePlayerInvitationDialog](../../../core/libs/prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.cpp) simulates dialog completion without displaying UI or sending invitations, so `scePlayerInvitationDialogGetResult` reports `USER_CANCELED`; its parameter ABI remains unverified. The 40-byte result layout (error code, result, 32 reserved bytes) is taken from [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/Dialog/PlayerInvitationDialog.cs) and the error codes from the other common dialogs, not confirmed on a title.
- [scePlayerInvitationDialogInitialize, scePlayerInvitationDialogOpen, scePlayerInvitationDialogTerminate](../../../core/libs/prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.cpp) (libScePlayerInvitationDialog) - the codes `NOT_INITIALIZED`, `ALREADY_INITIALIZED` and `ARG_NULL`, and the order of the `Open` checks (param first), were measured on the firmware 13.42 module. `Open` does not validate the contents of the param (size 0x88, user, mode, string at `+0x40`, zeroed tail), which the module rejects with `0x80B8000A`, does not return `BUSY` for `Open` while running, and `Initialize` does not return `0x80B80008` while another common dialog is in use.
