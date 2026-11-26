# libc

## change [fpurge]

- [fpurge](../../../core/libs/prx/libc/src/StreamPurge.cpp) (libc) discards buffered input and unwritten output, keeps a regular file's logical read position, pushed-back bytes, EOF and error status, as a PS5 payload process (firmware 11.60, probe `libc_fpurge`, build `d100643592f2`) did for one byte read, one pushed-back byte, EOF and an unwritten write; it reports `EBADF` for a closed stream. Not console-measured: repeated purges (host tests keep recorded pushback; other stream operations, including `feof` and `ferror`, invalidate it), several pushed-back bytes, pipes or terminals (which drop unread buffers and pushback), text mode, and failures: a rejected native pushback restore returns -1 keeping only the restored prefix, and on Linux a failed input synchronization before the reseek returns -1 and restores nothing. Null streams throw as existing stream wrappers do.
