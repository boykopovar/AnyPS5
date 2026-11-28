# libkernel

## change [pthread_key_create]

- [pthread_key_create](../../../core/libs/prx/libkernel/Pthread/Posix/Tsd.cpp) (libkernel) - an unwritable key pointer fails with `EFAULT` and frees the key it had taken, where FreeBSD 11.4's [`_pthread_key_create`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libthr/thread/thr_spec.c) stores the key without a check and the process faults. A PS5 (firmware 11.60, payload process; a title process may differ) returned 0 for a writable key pointer and the process took SIGSEGV for a NULL pointer, a pointer to a PROT_NONE page and an unmapped page in one run, so the console checks none of them, and the `EFAULT` here is a host-safety result, not the console's. A NULL key pointer keeps returning `EINVAL` here, which is also a host-safety result; `scePthreadKeyCreate` was not run there. `scePthreadKeyCreate` writes the key the same way before it publishes the slot, so an unwritable key returns `0x8002000e` and takes no key.
