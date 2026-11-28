# libc

## add Functional

- [libc++ character string insertion](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) implements the C-string, counted and single-character iterator `insert` overloads imported by Azahar (#5927). The iterator ABI uses a single element pointer. It returns a pointer into the resulting storage, including after growth. Full title and console behavior remain unverified.
