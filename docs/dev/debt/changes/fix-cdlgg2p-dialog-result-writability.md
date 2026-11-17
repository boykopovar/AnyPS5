# libSceCdlgG2P

## add Functional

- [sceG2PDialogGetResult](../../../core/libs/prx/libSceCdlgG2P/Export.cpp) (libSceCdlgG2P) - destination result prefix pointer is validated with GuestWritable_nid_no_patch upfront before writing cancel result with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
