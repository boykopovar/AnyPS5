# libSceUsbStorageDialog

## add Functional

- [sceUsbStorageDialogGetResult](../../../core/libs/prx/libSceUsbStorageDialog/Export.cpp) (libSceUsbStorageDialog) - destination result pointer in closed dialog branch is validated with GuestWritable_nid_no_patch upfront before modifying output fields with caller errno preserved; rejecting an unmapped result pointer is host safety fallback with console behavior unmeasured.
