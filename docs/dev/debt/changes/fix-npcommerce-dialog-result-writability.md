# libSceNpCommerce

## add Functional

- [sceNpCommerceDialogGetResult](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - destination result pointer is validated with GuestWritable_nid_no_patch upfront before writing cancel result fields with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
