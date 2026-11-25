# libScePosixForWebKit

## add Functional

- [setmode and getmode](../../../core/libs/prx/libScePosixForWebKit/SetMode.cpp) (libScePosixForWebKit) - console behavior is unverified. Follows FreeBSD 11.4 [`setmode.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/setmode.c) at the pinned commit. The mask comes from `umask` read by setting it to 0 and restoring it, not from the `sysctl` that FreeBSD tries first, and that read is not protected by blocking signals as in FreeBSD. The command list is guest heap memory that the caller frees. An empty or malformed mode, or an octal value with bits outside the standard set, returns null with `EINVAL`, and an octal value out of range returns null with `ERANGE`.
