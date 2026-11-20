# libkernel

## change [scePthreadMutexSetspinloops and scePthreadMutexSetyieldloops]

- [scePthreadMutexSetspinloops and scePthreadMutexSetyieldloops](../../../core/libs/prx/libkernel/Pthread/src/Mutex.cpp), also reached through `pthread_mutex_setspinloops_np` and `pthread_mutex_setyieldloops_np` (libkernel) - a mutex pointer that cannot be read fails with `SCE_KERNEL_ERROR_EFAULT` (`EFAULT` from the POSIX wrappers), like the getters. A null mutex pointer still throws and a destroyed mutex still returns `EINVAL`, in the earlier order. On a PS5 payload process (firmware 11.60) `scePthreadMutexSetspinloops(NULL, 1)` faulted with `SIGSEGV` and returned no code, where this throws; an unreadable non-null pointer and `scePthreadMutexSetyieldloops` were not measured, so the `EFAULT` here is a host-safety fallback. A read-only slot that holds a static initializer is still written by the lazy initialisation and is not checked.
