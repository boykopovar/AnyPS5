# libSceIcu

## add Functional

- [UTF-32 conversion buffers](../../../core/libs/prx/libSceIcu/Utf32.cpp) (libSceIcu) - `u_strFromUTF32WithSub` and `u_strToUTF32WithSub` (and their no-substitution forms) copy the source in once, a terminated one in page-bounded chunks, build the result on the host and write it, the length and the substitution count through checked copies. An unreadable source or unwritable output sets `U_ILLEGAL_ARGUMENT_ERROR`. The guest `UErrorCode` is read once with `GuestCopyIn_nid_no_patch` and written back with `GuestCopyOut_nid_no_patch` only when the conversion changes it; an unreadable or unwritable status throws an invalid-argument exception, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured.
