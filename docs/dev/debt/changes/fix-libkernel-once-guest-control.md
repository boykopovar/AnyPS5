# libkernel

## add Functional

- [pthread_once](../../../core/libs/prx/libkernel/Pthread/Posix/Once.cpp) (libkernel) - a non-null control pointer whose state field is unreadable or unwritable returns `EFAULT` (`14`) via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` without running the initializer; FreeBSD 11.4 dereferences the control structure and faults the process, so returning `EFAULT` is a host-safety fallback and is not measured on a console.
