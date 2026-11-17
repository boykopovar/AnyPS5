# libkernel

## change [cpuset_setaffinity]

- [cpuset_setaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - ROOT/CPUSET setters, numbered sets, jails, the WHICH base setter and interrupts throw. Affinity is bookkeeping for guest CPU reporting and inheritance, not host scheduling, and is unverified on a console. [`kern_cpuset_setaffinity`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_cpuset.c#L1206-L1314) supplies level/scope error ordering. The console returns `EPERM` for a CPU outside the process's set (rooklz, #7135); here the mask is clipped to `sceKernelGetAvailableCpumask` and 0 returned, with `EDEADLK` when none remain. Sizes 17 to 23 return `ERANGE`, as a PS5 payload process (firmware 11.60) answered 17 and 23 bytes. A non-null mask that cannot be read returns `EFAULT` as a host-safety fallback; the console's answer was not measured

## change [cpuset_getaffinity]

- [cpuset_getaffinity](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) - Numbered sets, jails and WHICH interrupt/domain masks throw. The base set at CPU_LEVEL_CPUSET and CPU_WHICH_CPUSET id -1 is assumed to be `sceKernelGetAvailableCpumask`, unverified on a console; CPU_LEVEL_ROOT answers `0xffff` as a PS5 payload process read it (rooklz, #7135); a title's root set was not measured. [`kern_cpuset_getaffinity`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/sys/kern/kern_cpuset.c#L1088-L1187) supplies target/level error ordering and the process-mask union, which misses threads not created through libkernel. Sizes other than 8 to 16 bytes return `ERANGE` before any other check, as the 13.60 kernel's syscall 487 handler does (kernel-13.60 @ 0xffffffff9ec2dae5..0xffffffff9ec2daf1, 34 returned at 0xffffffff9ec2dd66); 17 to 23 bytes were not measured on a console. An unwritable mask returns `EFAULT` as a host-safety fallback

## remove [cpuset_getaffinity and cpuset_setaffinity]
