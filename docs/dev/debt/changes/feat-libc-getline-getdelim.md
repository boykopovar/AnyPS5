# libc

## add Functional

- [getdelim and getline](../../../core/libs/prx/libc/src/StandardStreams.cpp) (libc) - console behavior is unverified. Implemented from POSIX.1-2008 over the stream's `fgetc` path with the stream locked. A null argument sets `EINVAL`, an allocation failure `ENOMEM` and an unwritable guest pointer `EFAULT`, and a read error mid-line returns -1 without writing the output. The stream's buffer is grown through `realloc`, so the caller frees it as it frees any guest allocation.
