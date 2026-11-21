# libkernel

## change [pthread_key_create]

- [pthread_key_create](../../../core/libs/prx/libkernel/Pthread/Posix/Tsd.cpp) (libkernel) - an unwritable key pointer fails with `EFAULT` and frees the key it had taken, where FreeBSD 11.4's [`_pthread_key_create`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libthr/thread/thr_spec.c) stores the key without a check and the process faults. This is a host-safety fallback with unmeasured console behaviour. A NULL key pointer keeps returning `EINVAL`. `scePthreadKeyCreate` writes the key the same way before it publishes the slot, so an unwritable key returns `0x8002000e` and takes no key.
