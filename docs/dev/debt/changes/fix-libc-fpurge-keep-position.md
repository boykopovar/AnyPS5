# libc

## change [fpurge]

- [fpurge](../../../core/libs/prx/libc/src/StreamPurge.cpp) (libc) discards buffered input and unwritten output, keeps the logical read position of a regular file and its pushed-back bytes, and preserves EOF and error status, as a PS5 payload process (firmware 11.60, probe `libc_fpurge`, build `d100643592f2`) did for one byte read, one pushed-back byte, EOF and an unwritten write; it reports `EBADF` for a closed stream. Status queries and `clearerr` retain recorded pushback in host tests before purge; these combinations are not console-measured. Other intervening stream operations invalidate the record. Not measured: several pushed-back bytes, a pipe or terminal (the unread buffer and the pushback are dropped there), and text mode. The Windows path resets the existing host UCRT buffer and repositions the descriptor; Linux and macOS use their native purge first. Null streams throw as existing stream wrappers do.
