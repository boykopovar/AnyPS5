# libc

## add Unknown function info

- [strerror_r buffer](../../../core/libs/prx/libc/src/Errors.cpp) (libc) - the message and the zero padding up to `buflen` (what `strncpy` plus the final terminator wrote) are copied out with `GuestCopyOut_nid_no_patch`, the padding in 256-byte blocks; an unwritable buffer throws the invalid-argument exception a null buffer of 23 bytes or more already throws, because the console writes through it and its behaviour on unmapped memory is unmeasured.
