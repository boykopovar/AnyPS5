# libSceNpEulaDialog

## add Functional

- [sceNpEulaDialogOpen](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) (libSceNpEulaDialog) - parameter pointer is probed with GuestCopyIn_nid_no_patch with caller errno preserved; rejecting an unmapped parameter is host safety fallback with console behavior unmeasured.
- [sceNpEulaDialogGetResult](../../../core/libs/prx/libSceNpEulaDialog/Export.cpp) (libSceNpEulaDialog) - destination result pointer is validated with GuestWritable_nid_no_patch upfront before status checks with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
