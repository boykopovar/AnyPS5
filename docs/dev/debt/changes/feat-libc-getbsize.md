# libSceLibcInternal

## add Functional

- [getbsize](../../../core/libs/prx/libSceLibcInternal/Blocksize.cpp) (libSceLibcInternal) - console behavior is unverified. Follows FreeBSD 11.4 [`getbsize.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/getbsize.c) at the pinned commit: `BLOCKSIZE` is read with `getenv`, a value below 512 or an unknown suffix warns through `warnx` and falls back to 512, and the header is returned in a static buffer as in FreeBSD. The function lives here, next to `warnx`, because libc does not link this library.
