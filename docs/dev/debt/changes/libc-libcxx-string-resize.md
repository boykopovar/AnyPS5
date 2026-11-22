# libc

## add Functional

- [libc++ character string sizing](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) implements `resize` and `push_back` for the character strings used by Azahar (#5927). Growth uses the guest allocator, and shrinking retains existing capacity. Full title and console behavior remain unverified.
