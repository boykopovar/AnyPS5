# libkernel

## add Functional

- [O_NONBLOCK on a Windows character device](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) - `fcntl` `F_SETFL` and `ioctl` `FIONBIO` throw on a console, a serial port or any other character device, except a null device that descriptors 0 to 2 hold when libkernel loads and its dups, which only record the flag, as FreeBSD 11.4's [`null_ioctl`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/dev/null/null.c) accepts `FIONBIO`. A null device opened later, such as the placeholder of a kqueue, a ksem, a libkernel_sys message queue or a `sceNetSocket` socket, and a dup of one, still throw (FreeBSD answers `ENOTTY` for the first three and makes a socket nonblocking), and no guest path opens the null device on Windows
