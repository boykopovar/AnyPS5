# libc

## add Unknown function info

- [fmtcheck](../../../core/libs/prx/libc/src/Fmtcheck.cpp) (libc) - follows FreeBSD 11.4's [`fmtcheck.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/fmtcheck.c). It returns the second format when the first has an unknown directive or a directive that differs, and it returns the first format when they match. A null second format returns null instead of faulting, as FreeBSD does when the first format has directives. The console's behaviour is not measured.
