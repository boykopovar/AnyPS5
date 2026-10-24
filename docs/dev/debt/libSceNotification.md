# libSceNotification technical debt

### Unknown function info

- [sceNotificationSend, sceNotificationSendById, sceNotificationShowPsButtonPersistentBanner, sceNotificationHidePsButtonPersistentBanner](../../../core/libs/prx/libSceNotification/Export.cpp) (libSceNotification) - notification sending and banner presentation interfaces throw through `NotImplemented_nid_no_patch`. Native notification service IPC and parameter structures remain unimplemented.
