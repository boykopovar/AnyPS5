# libkernel

## change [kern.proc.proc]

- [kern.proc.proc](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - `sysctl` {`CTL_KERN`, `KERN_PROC`, `KERN_PROC_PROC`} lists one process, the calling one, as the same 1,088-byte FreeBSD 11 amd64 `kinfo_proc` that `kern.proc.pid` builds, because the runtime hosts one guest process. A fourth element is ignored, as [`sysctl_kern_proc`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_proc.c) takes no pid for `KERN_PROC_PROC`, and more than four elements return `EINVAL`. A null output reports six records' size, the one record plus the five that FreeBSD 11.4's [`sysctl_kern_proc`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/kern_proc.c#L1452-L1468) adds to its estimate; what the console answers is unverified, as are how many processes and records it lists and whether its record is 1,088 or 1,096 bytes.
