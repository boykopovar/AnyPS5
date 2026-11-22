# libScePosixForWebKit

## add Functional

- [mkdtemp](../../../core/libs/prx/libScePosixForWebKit/TemporaryFile.cpp) (libScePosixForWebKit) - console behavior is unverified. Follows FreeBSD's [mkdtemp](https://github.com/freebsd/freebsd-src/blob/stable/9/lib/libc/stdio/mktemp.c) with the same template rules as `mkstemp`: the last six characters must be `X` (`EINVAL` otherwise), the directory is created with mode 0700, and a name that exists is retried through every remaining combination before `EEXIST` is returned.
- [getpwnam](../../../core/libs/prx/libScePosixForWebKit/UserDatabase.cpp) (libScePosixForWebKit) - console account database is unverified. Returns the same root account as `getpwuid` for the name `root` and null for every other name, so it matches the only account `getpwuid` reports. Throws on a null name.
