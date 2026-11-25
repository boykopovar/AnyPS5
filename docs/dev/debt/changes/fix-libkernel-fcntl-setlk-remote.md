# libkernel

## add Functional

- [fcntl F_SETLK_REMOTE](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) fails with `EPERM` for every lock on a descriptor that is not a guest socket and sets no lock (a guest socket keeps the libSceNet `EINVAL`, where FreeBSD would give `EPERM` first); the console's answer is not measured. As FreeBSD 11.4's [`kern_fcntl_freebsd`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/kern_descrip.c#L449-L455) does, it first copies the `struct flock` in (`EFAULT` for an unreadable pointer); [`kern_fcntl`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/kern_descrip.c#L600-L605) then checks `PRIV_NFS_LOCKD`, which [`prison_priv_check`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_jail.c#L3060-L3386) denies a jailed process. A title is modelled as jailed, as agreed on #11216
