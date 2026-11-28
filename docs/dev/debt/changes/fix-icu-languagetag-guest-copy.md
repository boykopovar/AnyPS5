# libSceIcu

## add Unknown function info

- [uloc_forLanguageTag and uloc_toLanguageTag](../../../core/libs/prx/libSceIcu/LanguageTag.cpp) (libSceIcu) - the guest `UErrorCode* status`, input string, `parsedLength` and result buffer are read with `GuestCopyIn_nid_no_patch` in page-bounded chunks and written with `GuestCopyOut_nid_no_patch` in one copy including the terminator; an unreadable or unwritable pointer throws the invalid-argument exception a null pointer throws, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured.
