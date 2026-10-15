# libSceErrorDialog technical debt

### Silent stubs

- [libSceErrorDialog](../../../core/libs/prx/libSceErrorDialog/Export.cpp) - the error dialog runs the state machine but shows nothing; the error code passed to `sceErrorDialogOpen` is only logged; sceErrorDialogServiceInitialize and sceErrorDialogServiceTerminate return 0
