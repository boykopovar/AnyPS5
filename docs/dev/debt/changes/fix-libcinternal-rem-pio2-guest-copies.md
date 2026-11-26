# libSceLibcInternal

## change [__kernel_rem_pio2]

- [__kernel_rem_pio2](../../../core/libs/prx/libSceLibcInternal/RemPio2.cpp) (libSceLibcInternal, NID `GLNDoAYNlLQ`) - the console module was not read, so its use of this code, signature and table is unverified. It is FreeBSD 11.4's [`k_rem_pio2.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/k_rem_pio2.c#L293-L443) ([prototype](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/msun/src/math_private.h#L794)) with the 690-chunk 2/pi table amd64's `LDBL_MAX_EXP` of 16384 selects, bit for bit with that source on x86-64. Inputs on which that code reads outside its arrays or overflows throw: precision outside 0 to 3, fewer than 1 or more than `20 - jk` chunks, an all-zero input, an infinite or NaN chunk, an `e0` past the table, a null or unreadable `x` and a null or unwritable `y`
