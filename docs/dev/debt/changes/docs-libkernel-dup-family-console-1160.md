# libkernel

## change [dup and dup2]

- [dup and dup2](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) throw for the combinations they do not model: `dup2` of a guest socket onto descriptors 0 to 2, whose host streams libc writes without the socket table. Guest sockets share one socket object between the duplicates; the numbers come from the host descriptor table like those of files. On a PS5 payload process (firmware 11.60) `dup` returned `EBADF` for -1 and 999 and `dup2` for a source of -1 or 999 and targets of -1, 1000000 and 0x7fffffff, a duplicate shared the file offset and started without `FD_CLOEXEC`, and `open` of a plain file failed with `EMFILE` after 632 descriptors, far below `getdtablesize` (12555), while a `dup` still succeeded; titles not measured. On Windows a guest `dup2` onto descriptor 1 or 2 keeps host logging on an `_fdopen` of a duplicate, and the guest `FILE` stays on the descriptor that was replaced; opening that duplicate throws (#7791)

# libkernel_sys

## change [_dup2]

- [_dup2](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) follows FreeBSD 11.4's [_dup2](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/sys/dup.2) and POSIX.1-2008 dup2: it forwards to libkernel's dup2, returning the target descriptor or setting errno and returning -1 on failure. It inherits libkernel dup2's limits (duplicating kqueue/ksem descriptors, or moving guest sockets onto standard descriptors, throws) and on a PS5 payload process (firmware 11.60) `_dup2` gave the same results as `dup2` in all 8 cases measured (the target, or `EBADF` for a bad source or target); titles not measured.
