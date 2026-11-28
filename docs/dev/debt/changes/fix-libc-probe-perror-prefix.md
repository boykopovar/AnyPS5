# libc

## add Unknown function info

- [perror prefix](../../../core/libs/prx/libc/src/Errors.cpp) (libc) - the prefix is copied in with `CopyGuestString_nid_no_patch` (up to 64 KiB) before it is printed; `perror` returns nothing, so an unreadable prefix is left out like a null or empty one and the message is still printed with `errno` kept, because the console's behaviour on unmapped memory is unmeasured.
