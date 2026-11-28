# libkernel

## add Functional

- [Creating entries under app0.zar](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - `sceKernelMkdir`, `mkdir` and [`sceKernelOpen` with `O_CREAT`](../../../core/libs/prx/libkernel/File/src/Open.cpp) see the archive. `mkdir` of a path that exists only in it fails with `EEXIST`, as FreeBSD 11.4's [`kern_mkdirat`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_syscalls.c#L3359-L3418) answers before any write check. A new name inside a directory that exists only in it fails with `EROFS`, as its [lookup](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_lookup.c#L937-L946) does for a create on a read-only mount, and with `ENOTDIR` when the archived parent is a file; opening a missing name there without `O_CREAT` stays `ENOENT`. Nothing is created on the host. The console's `/app0` PFS answers are not measured. CUSA12982 creates `/app0/engine` before writing its config files.

# libc

## add Functional

- [fopen creating under app0.zar](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) - `fopen` in a `w` or `a` mode of a new name inside a directory that exists only in the archive fails with `EROFS`, or `ENOTDIR` when the archived parent is a file, as the libkernel create does; `r+` of a missing name stays `ENOENT`. Not measured on a console.
