# libkernel

## add Functional

- [Spinlock](../../../core/libs/prx/libkernel/Pthread/Posix/Spinlock.cpp) `pthread_spin_init` and `pthread_spin_destroy` copy non-null `lock` pointers through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `EFAULT` (`14`) on unreadable or unwritable guest storage before leaving leaked allocations; `pthread_spin_lock`, `pthread_spin_trylock` and `pthread_spin_unlock` keep direct loads so per-iteration lock acquisition avoids checked-copy syscalls.
