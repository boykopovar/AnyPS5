# libc

## change [fpurge]

- [fpurge](../../../core/libs/prx/libc/src/StreamPurge.cpp) (libc): consecutive purges, status-query/`clearerr` combinations, restoration failures, several pushed-back bytes, pipes/terminals and text mode are not console-measured. Firmware 11.60 probe `libc_fpurge` build `d100643592f2` checked one read byte, one pushback, EOF and pending output. The host keeps regular-file logical position and pushback, discards buffered input and unwritten output, and preserves EOF/error. Host tests retain pushback across consecutive purges, status queries and `clearerr`; other intervening stream operations invalidate its record. A failed native pushback restore returns -1 and records only its restored prefix. Pipes/terminals drop unread input and pushback. Closed streams report `EBADF`; null streams throw. Windows resets the UCRT buffer and repositions the descriptor; Linux/macOS use native purge first.
