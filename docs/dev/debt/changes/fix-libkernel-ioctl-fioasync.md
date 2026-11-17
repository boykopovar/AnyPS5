# libkernel

## add Functional

- [ioctl FIOASYNC](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) - setting it on a pipe, on a host socket that is not a guest socket or on a character device other than the random device throws, as setting `O_ASYNC` there does, since `SIGIO` is not delivered; clearing a flag that is not set succeeds. On a file (one read from app0.zar included), a directory or the random device it sets or clears `O_ASYNC` from its argument, as FreeBSD 11.4's [`kern_ioctl`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/sys_generic.c#L828-L834) sets `FASYNC` before `vn_ioctl` and `randomdev_ioctl` return 0; not measured on a console
