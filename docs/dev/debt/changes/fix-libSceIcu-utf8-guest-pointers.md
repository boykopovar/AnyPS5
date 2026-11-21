# libSceIcu

## add Functional

- [utf8_appendCharSafeBody](../../../core/libs/prx/libSceIcu/Utf8.cpp) (libSceIcu) - isError guest pointer is written only when the append fails, as ICU 68.2's utf_impl.cpp does, and through GuestCopyOut_nid_no_patch; throwing on unmapped or unwritable pointer there is host safety fallback with console behavior unmeasured.

