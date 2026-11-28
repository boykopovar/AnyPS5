# libkernel

## add Unknown function info

- [sem_setname name](../../../core/libs/prx/libkernel/Pthread/Posix/Sem.cpp) (libkernel) - the name is copied in with `CopyGuestString_nid_no_patch` capped at 32 bytes, which also gives the existing `ENAMETOOLONG` for 32 or more characters; an unreadable name fails with `EINVAL` like a null one and leaves the semaphore unnamed, because the console's behaviour on unmapped memory is unmeasured.
