# libc

## add Unknown function info

- [strmode buffer](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - the 12-byte mode string is built in a local and written with one `GuestCopyOut_nid_no_patch`; an unwritable buffer is skipped (and `errno` left alone), because `strmode` returns nothing and its behaviour on unmapped memory is unmeasured.
