# libkernel

## change [cpuset_setaffinity]

- [cpuset_setaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - Interrupts, and a CPU_WHICH_CPUSET set the process has left (default set 1 after `cpuset`), throw: set IDs are not tied to masks, as `cpuset` and `cpuset_setid` keep the process mask. Which set IDs a title sees is not measured. Affinity is guest bookkeeping, unverified on a console. The console returns `EPERM` for a CPU outside the process's set (rooklz, #7135); here the mask is clipped to `sceKernelGetAvailableCpumask`, with `EDEADLK` when none remain. Root targets return `EPERM` and unknown set IDs `ESRCH`, as FreeBSD's `cpuset_which` and `cpuset_modify` do. Sizes 17 to 23 return `ERANGE`, as a PS5 payload process (firmware 11.60) answered. A jail returns `ESRCH` after the mask is read, as the 13.60 kernel's lookup (kernel-13.60 @ 0xffffffff9ec2c5e0) searches only jails below the caller's. An unreadable mask returns `EFAULT` as a host-safety fallback

## change [cpuset_getaffinity]

- [cpuset_getaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - WHICH interrupt/domain masks, and a CPU_WHICH_CPUSET set the process has left, throw (see cpuset_setaffinity). The base set is assumed to be `sceKernelGetAvailableCpumask`, unverified on a console; CPU_LEVEL_ROOT and root set 0 answer `0xffff` as a PS5 payload process read it (rooklz, #7135), and unknown set IDs return `ESRCH`, as FreeBSD's `cpuset_which` does. The order follows the 13.60 kernel's syscall 487 handler (kernel-13.60 @ 0xffffffff9ec2dac0): `ERANGE` for sizes outside 8 to 16 bytes first, then the target's `ESRCH` or `EINVAL`, then level errors, and a null mask's `EFAULT` last. 17 to 23 bytes were not measured. The process-mask union of [`kern_cpuset_getaffinity`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_cpuset.c#L1088-L1187) misses threads not created through libkernel

## remove [cpuset_getaffinity and cpuset_setaffinity]
