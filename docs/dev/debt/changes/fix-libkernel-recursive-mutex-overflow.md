# libkernel

## add Functional

- [scePthreadMutexLock, scePthreadMutexTrylock, scePthreadMutexTimedlock](../../../core/libs/prx/libkernel/Pthread/src/Mutex.cpp), also reached through `pthread_mutex_lock`, `pthread_mutex_trylock`, `pthread_mutex_timedlock` and `pthread_mutex_reltimedlock_np` (libkernel) - unmeasured on a console: a recursive mutex relocked by its owner at the largest count returns `EAGAIN` (35, `0x80020023` from the SCE forms) and keeps its state, as FreeBSD 11.4 libthr's [`mutex_self_trylock` and `mutex_self_lock`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libthr/thread/thr_mutex.c#L849-L945) do. The count here includes the first lock, so the depth stops one level below libthr's
- [scePthreadMutexLock](../../../core/libs/prx/libkernel/Pthread/src/Mutex.cpp), also reached through `pthread_mutex_lock` and `pthread_mutex_timedlock` without a deadline (libkernel) - a normal mutex (type 3) locked again by its owner throws, where FreeBSD 11.4 libthr's `mutex_self_lock` sleeps forever on purpose; unmeasured on a console. The timed forms sleep to the deadline and return `ETIMEDOUT`, as libthr does
