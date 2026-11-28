# libc

## add Functional

- [libc++ character string append](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) implements the null-terminated, counted, and repeated-character overloads imported by Azahar (#5927). The guest string retains available capacity and uses the guest allocator for growth. Full title and console behavior remain unverified.
