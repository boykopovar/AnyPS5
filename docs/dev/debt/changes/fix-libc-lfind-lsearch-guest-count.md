# libc

## add Unknown function info

- [lfind and lsearch element count](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) - the count is read once with `GuestCopyIn_nid_no_patch`, and `lsearch` writes the appended count with `GuestCopyOut_nid_no_patch` before it copies the key, so an unwritable count leaves the array unchanged; an unreadable or unwritable count throws the invalid-argument exception a null count throws, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. The key, the array and the comparator results are still accessed in place.
