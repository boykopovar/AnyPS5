# libc

## add Functional

- [getdelim and getline](../../../core/libs/prx/libc/src/StandardStreams.cpp) (libc) - console behavior is unverified. Implemented from POSIX.1-2008 over the stream's `fgetc` path with the stream locked. A null argument sets `EINVAL`, an allocation failure `ENOMEM` and an unwritable guest pointer `EFAULT`, and a read error mid-line returns -1 without writing the output. When the caller's buffer is too small, a new one replaces it and the old one is freed only after both pointers are updated; a failed guest write leaves the caller's pointer and capacity unchanged.
