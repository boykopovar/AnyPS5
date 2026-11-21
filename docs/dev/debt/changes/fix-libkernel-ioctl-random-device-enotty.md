# libkernel

## add Functional

- [ioctl on the random device](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) - `FIODTYPE` and `FIODGNAME` throw, as FreeBSD 11.4's [`devfs_ioctl_f`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/fs/devfs/devfs_vnops.c#L763-L779) answers them from the device's flags and name, which are not modelled for the console. Any other request without a case for every descriptor fails with `ENOTTY` on `/dev/random` and `/dev/urandom`, as [`randomdev_ioctl`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/dev/random/randomdev.c#L301-L316) refuses all but `FIOASYNC` and `FIONBIO`, after an input argument is copied in
