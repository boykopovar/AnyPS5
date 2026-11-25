# libc

## add Functional

- [libc++ string erase](../../../core/libs/prx/libc/src/LibcxxStringErase.cpp) (libc) implements the position-and-count overload imported by the Azahar core (#5927). It erases bytes in the existing inline or heap storage and preserves the long string's allocation even when the new size fits inline. Full title and firmware behavior are unmeasured.
