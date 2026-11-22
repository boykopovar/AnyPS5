# libc

## add Functional

- [libc++ string storage](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) (libc) implements the character string's reserve member and base destructor imported by Azahar (#5927). Long strings use the same guest new and delete exports as the caller. Reserve retains existing capacity or grows using the guest libc++ layout and 16-byte allocation rounding. Full title and console behavior remain unmeasured.
