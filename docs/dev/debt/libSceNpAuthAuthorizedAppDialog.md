# libSceNpAuthAuthorizedAppDialog technical debt

### Silent stubs

- [sceNpAuthAuthorizedAppDialogOpen](../../../core/libs/prx/libSceNpAuthAuthorizedAppDialog/Export.cpp) (libSceNpAuthAuthorizedAppDialog) - with `ANYPS5_PSN_DIALOGS=show`, a successful open shows AnyPS5's notice that the feature needs PlayStation Network; the console's own display without PlayStation Network is not measured, and the notice wording is AnyPS5's.
- [libSceNpAuthAuthorizedAppDialog](../../../core/libs/prx/libSceNpAuthAuthorizedAppDialog/Export.cpp) - the authorized-app dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no PSN sign-in to authorize an app against. Parameter fields at offsets 64 and 72 feed a 136-byte application record and a text value of at most 1024 bytes; their service-side meaning remains incomplete.

### Unknown function info

- [sceNpAuthAuthorizedAppDialogOpen](../../../core/libs/prx/libSceNpAuthAuthorizedAppDialog/Export.cpp) (libSceNpAuthAuthorizedAppDialog) - argument validation evaluating before the initialization check is emulator behavior and remains unmeasured on PS5 hardware.
