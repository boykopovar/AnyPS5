# libkernel

## add Unknown function info

- [pthread_cond_timedwait abstime](../../../core/libs/prx/libkernel/Pthread/Posix/Cond.cpp) (libkernel) - the deadline is copied in with `GuestCopyIn_nid_no_patch` and the wait uses the copy; an unreadable deadline returns `EINVAL` like the null one, because the console's behaviour on unmapped memory is unmeasured.
