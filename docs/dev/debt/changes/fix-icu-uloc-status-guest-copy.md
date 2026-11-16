# libSceIcu

## add Unknown function info

- [uloc_getVariant and uloc_setKeywordValue status](../../../core/libs/prx/libSceIcu/LocaleSyntax.cpp) (libSceIcu) - the guest `UErrorCode* status` is read once with `GuestCopyIn_nid_no_patch` and written back with `GuestCopyOut_nid_no_patch` only when ICU changes it; an unreadable or unwritable status throws the invalid-argument exception a null status throws, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. The locale, keyword and value strings and the result buffer are still accessed in place.
