# libkernel

## change [tcgetattr, tcsetattr]

- [tcgetattr, tcsetattr](../../../core/libs/prx/libkernel/File/src/Terminal.cpp) (libkernel) - every file and pipe descriptor is taken as not a terminal (`ENOTTY`), and a socket gets `EADDRNOTAVAIL` for `AF_INET` and `EOPNOTSUPP` otherwise, as FreeBSD's `in_control` and `pru_control_notsupp` answer a termios ioctl (a payload process on firmware 11.60 got from `tcgetattr` `EADDRNOTAVAIL` for descriptors 0 to 2, `EBADF` for -1 and 999, `ENOTTY` for `/dev/null` and pipe ends and `EOPNOTSUPP` for a local socket pair, and the buffer stayed unchanged; `tcsetattr` is unmeasured); what descriptors 0 to 2 of a title are on the console, and whether any of them is a terminal there, is unknown

## change [cfsetspeed]

- [cfsetspeed](../../../core/libs/prx/libkernel/File/src/Terminal.cpp) (libkernel, NID `C1MQ6OaXRjo`) - on 11.60 (payload) both fields were set, nothing else changed; versions unmeasured. The prototype, uint32 speed and 44-byte termios layout (input speed at 36, output speed at 40) follow the public PS5 payload SDK [termios.h](https://github.com/ps5-payload-dev/sdk/blob/a7685ce8e97ce1f8a4f38253a41d54241c9124f1/include/freebsd/termios.h) and [sys/_termios.h](https://github.com/ps5-payload-dev/sdk/blob/a7685ce8e97ce1f8a4f38253a41d54241c9124f1/include/freebsd/sys/_termios.h). The module association follows its libkernel stub table and the NID rehashes to the aerolib name. Both speed fields accept every uint32 value and no flags or control characters change, as [FreeBSD 11.4 termios.c](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/gen/termios.c) does. No terminal I/O is performed. Null pointers throw where the reference dereferences them.
