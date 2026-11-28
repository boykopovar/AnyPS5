# libc

## add Unknown function info

- [wcsftime time and buffer](../../../core/libs/prx/libc/src/Time.cpp) (libc) - the time is copied in with `GuestCopyIn_nid_no_patch`, and the text is formatted into a host buffer (at most 1 Mi UTF-16 units) and written with one `GuestCopyOut_nid_no_patch` covering exactly the units the direct writes covered; an unwritable buffer returns 0, the failure result, and an unreadable time throws the invalid-argument exception a null time throws, because the console's behaviour on unmapped memory is unmeasured. A size above 1 Mi units whose text needs 1 Mi units or more returns 0. The format is still read in place.
