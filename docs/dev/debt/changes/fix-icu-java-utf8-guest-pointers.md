# libSceIcu

## add Unknown function info

- [u_strFromJavaModifiedUTF8WithSub and u_strToJavaModifiedUTF8 guest pointers](../../../core/libs/prx/libSceIcu/JavaUtf8.cpp) (libSceIcu) - the status is read once and written back only when it changes, the source is copied in with `GuestCopyIn_nid_no_patch` (scanned in 4 KiB-aligned chunks up to its terminator when the length is -1), and the converted text, substitution count and length are each written with one `GuestCopyOut_nid_no_patch`; an unreadable or unwritable pointer throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured.
