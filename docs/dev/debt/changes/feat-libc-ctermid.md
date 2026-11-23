# libc

## add Unknown function info

- [ctermid](../../../core/libs/prx/libc/src/ControllingTerminal.cpp) (libc) - returns `/dev/tty`, the FreeBSD 11.4 [`_PATH_TTY`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/paths.h), into a static buffer of 1024 bytes (FreeBSD [`L_ctermid`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/stdio.h)) for a null argument, or into the caller's buffer. FreeBSD's [`ctermid`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/ctermid.c) then replaces the name with the host device when `/dev/tty` is a character device, using `stat` and `sysctl kern.devname`; this port does not, and the console's answer is not measured. A caller buffer that cannot be written returns null with `EFAULT`, the convention of this library, since POSIX defines no error return.
