# libkernel

## add Functional

- [O_SHLOCK and O_EXLOCK in open](../../../core/libs/prx/libkernel/File/src/Open.cpp) (libkernel) - throw after acquiring a lock on a regular file opened with `O_RDONLY | O_TRUNC`, on the random device and on files or directories read from app0.zar; the host cannot truncate through a read-only descriptor and the console's answer for the others is unknown. Otherwise `flock` follows FreeBSD 11.4's [`vn_open_vnode`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/vfs_vnops.c) and `O_TRUNC` runs after the lock as in [`kern_openat`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/vfs_syscalls.c). Other guest threads can see the descriptor before the lock and truncate finish. Not measured on a PS5.
