# libc

## add Unknown function info

- [strftime time and buffer](../../../core/libs/prx/libc/src/Time.cpp) (libc) - the time is copied in with `GuestCopyIn_nid_no_patch`, and the text is formatted into a host buffer (at most 1 MiB) and written with one `GuestCopyOut_nid_no_patch` covering exactly the bytes the direct writes covered; an unwritable buffer returns 0, the failure result, and a null or unreadable time throws the invalid-argument exception `wcsftime` throws for a null time, because the console's behaviour on unmapped memory is unmeasured. The format is still read in place.
