# libScePosixForWebKit

## add Unknown function info

- [glob and globfree](../../../core/libs/prx/libScePosixForWebKit/Glob.cpp) (libScePosixForWebKit) - the 88-byte `glob_t` layout (pathc, matchc, offs, flags, pathv, errfunc and five alternate-access hooks) follows the FreeBSD 11.4 [`glob.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/glob.h), not verified on a PS5. Matching uses this library's `fnmatch` and the guest `opendir`, `readdir` and `stat`; the console's result order, GLOB_MARK, GLOB_NOCHECK and error reporting are unmeasured. GLOB_ALTDIRFUNC, GLOB_BRACE, GLOB_QUOTE, GLOB_TILDE, GLOB_LIMIT and any other unknown flag bit throw. An opendir failure other than ENOENT or ENOTDIR calls the errfunc and, with GLOB_ERR or a nonzero errfunc result, returns GLOB_ABORTED. If publishing results runs out of memory, GLOB_NOSPACE is returned and the strings already allocated for that call are not freed.
