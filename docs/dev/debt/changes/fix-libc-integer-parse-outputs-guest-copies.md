# libc

## add Functional

- [_Stolx, _Stollx, _Stoulx and _Stoullx](../../../core/libs/prx/libc/src/DinkumwareInternals.cpp) (libc) write their error output, and the end pointer they set for an invalid base, with `GuestCopyOut_nid_no_patch`, so an output that can't be written throws `std::invalid_argument`. This is a host-safety fallback; the console's behaviour for an inaccessible output is not measured. For a valid base the end pointer is still written directly, by `strtoll` or `strtoull`, and the floating-point and `__sce_v2` parsers in the same file still write their outputs directly.
