# libSceNpAuthAuthorizedAppDialog

## add Functional

- [sceNpAuthAuthorizedAppDialogGetResult](../../../core/libs/prx/libSceNpAuthAuthorizedAppDialog/Export.cpp) (libSceNpAuthAuthorizedAppDialog) - destination result pointer is validated with GuestWritable_nid_no_patch upfront before writing cancel result with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
