# libkernel

## add Functional

- [O_SHLOCK and O_EXLOCK in open](../../../core/libs/prx/libkernel/File/src/Open.cpp) (libkernel) - on Linux and Windows, a locked regular file opened with `O_RDONLY | O_TRUNC` throws after acquiring the lock. Those hosts cannot truncate through the read-only descriptor. Their separate open, lock and truncate steps can expose the descriptor to other guest threads before completion. Darwin uses native BSD locking open flags for host files. Locked opens on the random device and on files or directories read from app0.zar remain unsupported. The reference order is FreeBSD 11.4's [`vn_open_vnode`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/vfs_vnops.c) and [`kern_openat`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/vfs_syscalls.c). The console's behavior has not been measured.
