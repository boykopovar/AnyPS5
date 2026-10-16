# libSceWebBrowserDialog technical debt

### Silent stubs

- [libSceWebBrowserDialog](../../../core/libs/prx/libSceWebBrowserDialog/Export.cpp) has no host browser runtime. `sceWebBrowserDialogOpenForPredeterminedContent` completes immediately with `FINISHED`, while `sceWebBrowserDialogSetCookie` and `sceWebBrowserDialogResetCookie` accept calls after initialization without storing cookies. The dialog status and error ordering follow the existing `sceWebBrowserDialogOpen` path; browser rendering, navigation and cookie persistence remain unavailable
