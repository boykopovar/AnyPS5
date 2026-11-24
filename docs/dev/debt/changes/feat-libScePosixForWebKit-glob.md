# libScePosixForWebKit

## add Unknown function info

- [glob and globfree](../../../core/libs/prx/libScePosixForWebKit/Glob.cpp) (libScePosixForWebKit) - the 88-byte `glob_t` follows FreeBSD 11.4 [`glob.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/glob.h), not verified on a PS5. Matching uses this library's `fnmatch`, `opendir`, `readdir` and `stat`; the console's result order, GLOB_MARK, GLOB_NOCHECK and error reporting are unmeasured. Separators are kept as written, as FreeBSD [`glob.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/glob.c) does. GLOB_ALTDIRFUNC, GLOB_BRACE, GLOB_QUOTE, GLOB_TILDE, GLOB_LIMIT and other unknown bits throw. Matches found before GLOB_ABORTED are published. Out of memory returns GLOB_NOSPACE, frees that call's strings and leaves the guest array unchanged.
