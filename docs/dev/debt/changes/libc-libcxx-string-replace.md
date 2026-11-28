# libc

## add Functional

- [libc++ character string replacement](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) implements the counted and repeated-character `replace` overloads imported by Azahar (#5927). It clips the removed range at the string's end and preserves aliased input when moving the suffix. Full title and console behavior remain unverified.
