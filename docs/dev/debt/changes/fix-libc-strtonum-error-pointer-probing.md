# libc

## add Unknown function info

- [strtonum error pointer](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - safely probe guest error pointer via `GuestCopyOut_nid_no_patch`, setting `errno = 14` (EFAULT) and returning 0 for unmapped or unwritable memory as a host-safety fallback; retail console fault behavior has not been measured.
