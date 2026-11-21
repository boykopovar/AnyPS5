# libkernel

## add Functional

- [pthread_spin_destroy](../../../core/libs/prx/libkernel/Pthread/Posix/Spinlock.cpp) (libkernel) - destroying a spinlock whose `owner` is set fails with `EBUSY` (`16`) without clearing `*lock` or deleting the `PthreadSpinlockPrivate` allocation, where FreeBSD 11.4's [`_pthread_spin_destroy`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libthr/thread/thr_pspinlock.c#L77-L96) frees `*lock` and stores `NULL` without checking `s_lock.m_owner`. Returning POSIX's optional `EBUSY` avoids a host C++ use-after-free on `std::atomic<std::thread::id>` while another thread holds or spins on the lock; PS5 console behaviour is unmeasured.
