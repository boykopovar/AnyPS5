# libc

## add Functional

- [clogl](../../../core/libs/prx/libc/src/MathExact.cpp) (libc) - near the unit circle the log magnitude uses compensated squares and sums adapted from [FreeBSD msun](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/s_clogl.c). At extreme scales it uses the ratio of the smaller component to the larger one. Branch cuts and special values have not been measured on console.
