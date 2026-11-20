# libkernel

## change [kern.proc.env]

- [kern.proc.env](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - which variables, if any, the console starts an application with is unverified; `sysctl` {`CTL_KERN`, `KERN_PROC`, `KERN_PROC_ENV`, pid} returns 0 bytes, the empty start-up environment `environ` and libc's `getenv` also begin with, as FreeBSD 11's [`sysctl_kern_proc_env`](https://github.com/freebsd/freebsd-src/blob/releng/11.0/sys/kern/kern_proc.c) copies a process's start-up strings, not later `setenv` changes. Other processes' pids and -1 fail with `ESRCH`; a write returns `EPERM`. PS5VR (PPSA99177) reads it for its own pid at start-up.

## change [kern.proc.vmmap]

- [kern.proc.vmmap](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - whether a PS5 application may read its own map, and what types and paths the console reports, is unverified. `sysctl` {`CTL_KERN`, `KERN_PROC`, `KERN_PROC_VMMAP`, pid} returns one 144-byte `kinfo_vmentry`, as [`kinfo_getvmmap`](https://github.com/freebsd/freebsd-src/blob/releng/11.0/lib/libutil/kinfo_getvmmap.c) reads them, per used range of the guest address-space arena, in address order: type `KVME_TYPE_DEFAULT`, the protection `sceKernelVirtualQuery` gives the range's first page (0 where none), and 0 offset, object, resident page counts, flags and an empty path. Ranges outside the arena are not reported. The arena lock is taken per range, not for the whole walk as FreeBSD holds the map lock, so another thread's `mmap` or `munmap` can change the list between two records. Other processes' pids and -1 fail with `ESRCH`, a write with `EPERM` and a short buffer with `ENOMEM`.
