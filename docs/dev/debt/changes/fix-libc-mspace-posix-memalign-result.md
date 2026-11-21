# libc

## add Unknown function info

- [sceLibcMspacePosixMemalign result](../../../core/libs/prx/libc/src/Mspace.cpp) (libc) - the allocated pointer is written with `GuestCopyOut_nid_no_patch`; an unwritable `result` releases the block again and throws `std::invalid_argument` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured.
