# libc

## add Unknown function info

- [_Thrd_start_with_name name](../../../core/libs/prx/libc/src/Threads.cpp) (libc) - the first byte of the name (which decides between a named and an unnamed thread) is read with `GuestCopyIn_nid_no_patch`; an unreadable name returns `thrd_error` (4) without creating a thread, because the console's behaviour on unmapped memory is unmeasured. The name itself is still copied by `scePthreadCreate`.
