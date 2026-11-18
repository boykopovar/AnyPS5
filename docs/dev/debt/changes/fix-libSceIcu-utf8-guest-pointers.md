# libSceIcu

## add Functional

- [utf8_appendCharSafeBody](../../../core/libs/prx/libSceIcu/Utf8.cpp) (libSceIcu) - isError guest pointer is validated with GuestWritable_nid_no_patch and copied through GuestCopyOut_nid_no_patch; throwing on unmapped or unwritable pointer is host safety fallback with console behavior unmeasured.

