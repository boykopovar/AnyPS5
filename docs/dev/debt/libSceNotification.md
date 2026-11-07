# libSceNotification technical debt

### Unknown function info

- [sceNotificationSend, sceNotificationSendById, sceNotificationShowPsButtonPersistentBanner, sceNotificationHidePsButtonPersistentBanner](../../../core/libs/prx/libSceNotification/Export.cpp) (libSceNotification) - notification sending and banner presentation interfaces throw through `NotImplemented_nid_no_patch`. Native notification service IPC and parameter structures remain unimplemented.
- [sceNotificationSend](../../../core/libs/prx/libSceNotification/Export.cpp) (libSceNotification) - what the console shows, checks and returns is not measured. The parameters are the ones homebrew built with the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk) declares, `int sceNotificationSend(int userId, bool isLogged, const char* payload)`: its `notify` sample passes the system user (`0xFE`), `true` and a JSON document whose `rawData` names a `viewTemplateType` such as `InteractiveToastTemplateB` and holds the `viewData` to show, and exits with the result. Closes when: measured on a PS5.
