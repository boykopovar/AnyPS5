# libkernel

## add Functional

- [pthread_cond_init, pthread_cond_destroy, pthread_cond_setname_np and scePthreadCondInit, scePthreadCondDestroy](../../../core/libs/prx/libkernel/Pthread/src/Cond.cpp) (libkernel) - a condition variable pointer that cannot be read (or written where the call stores), or an attribute pointer that cannot be read, fails with `EFAULT` (`SCE_KERNEL_ERROR_EFAULT` from the `sce` calls), where FreeBSD 11.4's [`_pthread_cond_init` and `_pthread_cond_destroy`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libthr/thread/thr_cond.c) store through the pointer unchecked and the process faults; `pthread_cond_setname_np` has no libthr counterpart and follows the same rule. Host-safety fallbacks with unmeasured console behaviour. A null condition variable pointer still throws; signal, broadcast, wait and `scePthreadCondSignalto` stay unchecked because they are hot paths.
