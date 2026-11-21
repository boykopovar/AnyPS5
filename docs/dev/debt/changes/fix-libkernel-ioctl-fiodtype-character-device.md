# libkernel

## add Functional

- [ioctl on a character device](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) - `FIODTYPE` answers 0 without knowing the console's devices: what descriptors 0 to 2 of a title are there is unknown, a host terminal answers 0 and not `D_TTY`, as every termios request fails with `ENOTTY` here, and a libkernel_sys message-queue descriptor, kept on the host null device, answers 0 where FreeBSD 11.4's [`invfo_ioctl`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_descrip.c#L4045-L4050) fails with `ENOTTY`. On the random device and a ksem id it still throws. Otherwise it follows [`devfs_ioctl_f`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/fs/devfs/devfs_vnops.c#L763-L767), which answers the device's `d_flags & D_TYPEMASK`, and the [null device](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/dev/null/null.c#L66-L72) sets none of those flags
