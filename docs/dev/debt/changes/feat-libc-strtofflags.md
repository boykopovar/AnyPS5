# libc

## add Functional

- [strtofflags](../../../core/libs/prx/libc/src/StatFlags.cpp) (libc) - console and PS5 flag values are unverified. Follows FreeBSD 11.4 [`strtofflags.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/strtofflags.c) at the pinned commit, with the `UF_*` and `SF_*` values from [`sys/sys/stat.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/sys/stat.h) there. Words are separated by tab, space or comma, as `strsep` does here. An unknown word returns 1 with the set and clear masks found before it and `*stringp` at that word. `fflagstostr` is not implemented.
