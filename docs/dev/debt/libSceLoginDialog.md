# libSceLoginDialog technical debt

### Silent stubs

- [libSceLoginDialog](../../../core/libs/prx/libSceLoginDialog/Export.cpp) - the login dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled
- [sceLoginDialogClose](../../../core/libs/prx/libSceLoginDialog/Export.cpp) - sceLoginDialogClose transitions an initialized dialog directly to COMMON_DIALOG_STATUS_FINISHED even when called before Open, following the Signin / NetCtlAp / PlayerSelection convention; this behavior is unverified against real hardware.

### Unknown function info

- [sceLoginDialogOpen, sceLoginDialogGetResult](../../../core/libs/prx/libSceLoginDialog/Export.cpp) - the 64-byte input and 16-byte result layouts are recovered. The roles of the nine user-ID slots and the eight trailing result bytes require service-side analysis. The UI stub returns the native disconnected-service cancellation result and does not populate successful login data.
