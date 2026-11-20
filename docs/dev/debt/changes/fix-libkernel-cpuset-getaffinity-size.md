# libkernel

## change [cpuset_setaffinity]

- [cpuset_setaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - ROOT/CPUSET setters, numbered sets, the WHICH base setter and interrupts throw. Affinity is guest bookkeeping, not host scheduling, unverified on a console. [`kern_cpuset_setaffinity`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_cpuset.c#L1206-L1314) gives the error order. The console returns `EPERM` for a CPU outside the process's set (rooklz, #7135); here the mask is clipped to `sceKernelGetAvailableCpumask` and 0 returned, with `EDEADLK` when none remain. Sizes 17 to 23 return `ERANGE`, as a PS5 payload process (firmware 11.60) answered 17 and 23 bytes. A jail target returns `ESRCH` after the mask is read, as the 13.60 kernel's target lookup (kernel-13.60 @ 0xffffffff9ec2c5e0) searches only jails below the caller's (0xffffffff9efd9ae0), which a title cannot create. An unreadable mask returns `EFAULT` as a host-safety fallback

## change [cpuset_getaffinity]

- [cpuset_getaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - Numbered sets and WHICH interrupt/domain masks throw. The base set at CPU_LEVEL_CPUSET and CPU_WHICH_CPUSET id -1 is assumed to be `sceKernelGetAvailableCpumask`, unverified on a console; CPU_LEVEL_ROOT answers `0xffff` as a PS5 payload process read it (rooklz, #7135). The order follows the 13.60 kernel's syscall 487 handler (kernel-13.60 @ 0xffffffff9ec2dac0): sizes other than 8 to 16 bytes return `ERANGE` first (0xffffffff9ec2dae5..0xffffffff9ec2daf1), then the target lookup's `ESRCH` or `EINVAL` (a jail always `ESRCH`, see cpuset_setaffinity), then level errors, and a null mask `EFAULT` last. 17 to 23 bytes were not measured on a console. [`kern_cpuset_getaffinity`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_cpuset.c#L1088-L1187) supplies the process-mask union, which misses threads not created through libkernel

## remove [cpuset_getaffinity and cpuset_setaffinity]
