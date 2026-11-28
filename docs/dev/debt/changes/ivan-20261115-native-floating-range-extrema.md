# libSceLibcInternal

## change [Floating-point min/max pair and element helpers]

- [Floating-point min/max pair and element helpers](../../../core/libs/prx/libSceLibcInternal/VectorAlgorithms.cpp) (libSceLibcInternal) - other Sony builds and console execution are unmeasured. The eight value-pair and element forms follow host replay of PPSA28050 v01.002.000's bundled `libc.prx` (SHA-256 `1f440564a488e0c735c87e6513c71c3ada6d1a23809df9f581c797ceb7463faa`; RVAs `0xf7380`, `0xf74f0`, `0xf7dd0`, `0xf7f40`, `0xf8c00`, `0xf8e50`, `0xfc4b0`, `0xfc560`).
  - A missing matching-lane mask throws where native code uses an indeterminate lane or invalid stack index. Double minimum/maximum element selection retains its initialized lane-zero fallback. These unresolved native paths have no supported successful result.
  - Float indexing retains the native 64-GiB chunk limit, but crossing that boundary and unmasked floating-point exception delivery are unmeasured. Invalid geometry and empty value ranges throw instead of reproducing out-of-range native reads; unmapped addresses in an otherwise valid range are not validated.
