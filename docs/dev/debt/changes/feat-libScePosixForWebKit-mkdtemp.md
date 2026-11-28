# libScePosixForWebKit

## add Functional

- [mkdtemp](../../../core/libs/prx/libScePosixForWebKit/TemporaryFile.cpp) (libScePosixForWebKit) - console behavior is unverified. Follows FreeBSD's [mkdtemp](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdio/mktemp.c) with the same template rules as `mkstemp`: every trailing `X` is replaced, a template with no trailing `X` is used as written, the directory is created with mode 0700, and a name that exists is retried through every remaining combination before `EEXIST` is returned. A template without a trailing `X` is not rejected, unlike POSIX, which requires `XXXXXX`.
- [getpwnam](../../../core/libs/prx/libScePosixForWebKit/UserDatabase.cpp) (libScePosixForWebKit) - console account database is unverified. Returns the same root account as `getpwuid` for the name `root` and null for every other name, so it matches the only account `getpwuid` reports. Throws on a null name.
