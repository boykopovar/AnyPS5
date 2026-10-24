# libSceNpAuthAuthorizedAppDialog technical debt

### Silent stubs

- [libSceNpAuthAuthorizedAppDialog](../../../core/libs/prx/libSceNpAuthAuthorizedAppDialog/Export.cpp) - the authorized-app dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as there is no PSN sign-in to authorize an app against. Parameter fields at offsets 64 and 72 feed a 136-byte application record and a text value of at most 1024 bytes; their service-side meaning remains incomplete.
