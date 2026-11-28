# libScePosixForWebKit

## add Unknown function info

- [mkstemps](../../../core/libs/prx/libScePosixForWebKit/TemporaryFile.cpp) (libScePosixForWebKit) - follows FreeBSD 11.4 [`mktemp.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdio/mktemp.c) at the pinned commit for the suffix bounds: `EINVAL` for a negative suffix length, a suffix length of the template length or more, or a suffix that contains `/`. The `X` characters before the suffix are replaced as in `mkstemp`. The console's behaviour is not measured, and which console module exports the symbol is not verified.
