# libkernel

## add Functional

- [sem_init_nid_postfix, sem_destroy_nid_postfix, and sem_getvalue_nid_postfix](../../../core/libs/prx/libkernel/Pthread/Posix/Sem.cpp) (libkernel) - writing guest semaphore handles and values through `GuestCopyOut_nid_no_patch` and returning `-1` with `errno = EINVAL` on unwritable pointers is a host-safety fallback; console behaviour on unmapped pointers is unmeasured.
