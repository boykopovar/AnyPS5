# libc

## add Functional

- [strtonum input pointer](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - copies the input through checked guest reads before parsing. A null, unreadable or unterminated input followed by unreadable memory reports `EFAULT` and the existing `invalid` error string. The copy grows as needed and allocation failure reports `ENOMEM`. Console behavior for invalid input memory is unverified.
