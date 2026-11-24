# libc

## add Unknown function info

- [fmtmsg](../../../core/libs/prx/libc/src/Fmtmsg.cpp) (libc) - console and PS5 header values are unverified. Follows FreeBSD's [`fmtmsg.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/fmtmsg.c) and [`fmtmsg.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/fmtmsg.h) at the pinned commit: the return codes, component order from `MSGVERB`, the tag placed after a space on the line built so far, empty `MSGVERB` fields skipped, and an overlong or invalid `MSGVERB` selecting all components. `MM_CONSOLE` writes to no console: it returns `MM_OK` when there is nothing to write and `MM_NOCON` otherwise. A write error on stderr is ignored, as FreeBSD does. A severity other than `MM_HALT`, `MM_ERROR`, `MM_WARNING` or `MM_INFO` prints no keyword. `addseverity` and the `SEV_LEVEL` environment variable are not implemented.
