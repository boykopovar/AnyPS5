# libc

## add Unknown function info

- [mbstowcs_s and mbsrtowcs_s guest outputs](../../../core/libs/prx/libc/src/BoundsChecking.cpp) (libc) - the converted wide characters are collected in a host buffer and written, with the terminator, in one `GuestCopyOut_nid_no_patch`, and the result and (for `mbsrtowcs_s`) the source pointer are read and written the same way, in the same order as before; an unreadable source pointer or an unwritable buffer, result or source pointer is reported as the same "bad argument" runtime-constraint violation (EINVAL) a null pointer gets, storing the failed result when it can, which is a host-safety fallback; whether the console module faults on unmapped memory is unmeasured. The multibyte text itself is still read in place.
