# libScePosixForWebKit

## add Unknown function info

- [hstrerror](../../../core/libs/prx/libScePosixForWebKit/HostError.cpp) (libScePosixForWebKit) - follows FreeBSD 11.4's [`herror.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/resolv/herror.c) at the pinned commit: it returns the five resolver messages for codes 0 to 4, `Unknown resolver error` for larger codes and `Resolver internal error` for negative ones. The console's message text is not measured, and `herror` is not implemented because the console's `h_errno` accessor is not known.
