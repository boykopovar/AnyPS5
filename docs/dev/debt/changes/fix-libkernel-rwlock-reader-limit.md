# libkernel

## add Functional

- [scePthreadRwlockRdlock, Tryrdlock, Timedrdlock and the pthread_rwlock read forms](../../../core/libs/prx/libkernel/Pthread/src/Rwlock.cpp) (libkernel) - the console's reader limit is not measured: a read lock that the lock would grant fails with `EAGAIN` (35, `SCE_KERNEL_ERROR_EAGAIN`) and changes nothing when 0x1fffffff read locks are held, as FreeBSD 11.4's [`URWLOCK_MAX_READERS`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/sys/umtx.h) check in [`_thr_rwlock_tryrdlock`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libthr/thread/thr_umtx.h) and [`do_rw_rdlock`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_umtx.c) does
