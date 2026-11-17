# libkernel

## add Unknown function info

- [POSIX timed-wait deadlines](../../../core/libs/prx/libkernel/Pthread/Posix/Common.hpp) (libkernel) - `RelativeMicroseconds`, which `pthread_mutex_timedlock`, `pthread_rwlock_timedrdlock`, `pthread_rwlock_timedwrlock` and `sem_timedwait` use to turn the deadline into a wait, copies the deadline in with `GuestCopyIn_nid_no_patch`; an unreadable deadline returns `EINVAL` like an out-of-range one, because the console's behaviour on unmapped memory is unmeasured.
