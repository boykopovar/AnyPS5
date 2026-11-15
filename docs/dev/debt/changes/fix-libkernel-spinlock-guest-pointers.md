# libkernel

## add Functional

- [pthread_spin_init, pthread_spin_destroy, pthread_spin_trylock, pthread_spin_lock, pthread_spin_unlock](../../../core/libs/prx/libkernel/Pthread/Posix/Spinlock.cpp) (libkernel) - a non-null `lock` pointer that cannot be read, or that cannot be written in `pthread_spin_init` or `pthread_spin_destroy`, fails with `EFAULT` (`14`) and preserves host `errno` via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, without leaking or freeing the handle; null `lock` pointers keep `EINVAL`. FreeBSD 11.4 [`thr_pspinlock.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libthr/thread/thr_pspinlock.c) dereferences `*lock`, so `EFAULT` is a host-safety fallback with unmeasured console behaviour.
