# libkernel_sys

## add Functional

- [scePthreadSuspend, scePthreadSuspendAll and scePthreadSingle](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - each is taken to be `pthread_suspend_np`, `pthread_suspend_all_np` or `pthread_single_np` under its sce name, as `scePthreadResume`, `scePthreadResumeAll` and `scePthreadMulti` are on the resume side; the pairing and the `Pthread` argument are unmeasured. `scePthreadSuspend` throws where `pthread_suspend_np` returns an error (a null, finished or calling thread), as its sce error values are unmeasured. `scePthreadSuspendAll` returns nothing, as `pthread_suspend_all_np` in FreeBSD 11.4 libthr ([`thr_suspend_np.c`](https://github.com/freebsd/freebsd-src/blob/8c639945/lib/libthr/thread/thr_suspend_np.c#L104-L157)) and `scePthreadResumeAll` do; the console form's return type is unmeasured. Closes when: measured on a PS5. Labels: console
