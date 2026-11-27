# libc

## add Functional

- [libc++ string character searches](../../../core/libs/prx/libc/src/LibcxxStringSearch.cpp) (libc) implement the `find(char, size_type)` and `rfind(char, size_type)` imports in RetroArch Azahar (PPSA99169, #5927). Both use the 24-byte layout observed in the guest and return a 64-bit `npos` for an absent match. Tests cover embedded zero bytes, position limits, and long strings. The firmware provider and full title behavior are unmeasured.
