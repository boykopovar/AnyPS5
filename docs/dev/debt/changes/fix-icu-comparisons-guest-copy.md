# libSceIcu

## add Functional

- [u_strlen, u_strcmp, u_strncmp, u_memcmp and u_memcmpCodePointOrder](../../../core/libs/prx/libSceIcu/Comparisons.cpp) (libSceIcu) - read guest UTF-16 code units through `GuestCopyIn_nid_no_patch` while preserving `errno`, throwing `std::invalid_argument` on unreadable guest pointers as a host-safety fallback instead of faulting; console behavior on invalid pointers is unmeasured.
