# libc

## add Functional

- [lgamma_r, lgammaf_r, gamma_r and gammaf_r](../../../core/libs/prx/libc/src/Math.cpp) (libc) write the sign of the gamma function with `GuestCopyOut_nid_no_patch`, so a sign output that can't be written throws `std::invalid_argument`, as a null one does. This is a host-safety fallback; the console's behaviour for an inaccessible output is not measured.
