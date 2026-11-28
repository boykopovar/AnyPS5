# libc

## change [operator new](../../../core/libs/prx/libc/src/StdioExtras.cpp)

- [operator new](../../../core/libs/prx/libc/src/StdioExtras.cpp) (libc) - the aligned forms throw `std::bad_alloc` (and the aligned `nothrow` forms return null) for an alignment of zero or one that is not a power of two, where the `libc.prx` bundled with PPSA21564 (build `J03093718`) calls `abort`; the `nothrow` forms catch every exception and return null, as that binary does.
