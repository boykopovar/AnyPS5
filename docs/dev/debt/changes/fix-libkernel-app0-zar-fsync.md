# libkernel

## add Functional

- [fsync and fdatasync on app0.zar files](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - `sceKernelFsync`, `sceKernelFdatasync`, `fsync` and `fdatasync` return 0 for a file read from app0.zar, including a descriptor that libc `fopen` opened on one, without touching the host. FreeBSD 11.4's [`kern_fsync`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_syscalls.c#L3097-L3137) does not require a writable descriptor, and a read-only file on disk already succeeds here; what the console's PFS answers is not measured. CUSA01073 opens `commandline.txt` from app0.zar and then calls `fsync` on an archived descriptor.
