# libc

## add Functional

- [_Xtime_diff_to_ts and std::__sce_v2::_Xtime_diff_to_millis2](../../../core/libs/prx/libc/src/TimeExtras.cpp) (libc) copy their `Xtime` arguments in with `GuestCopyIn_nid_no_patch` before computing, so a null or unreadable time throws `std::invalid_argument`, as the `__sce_v2` `Timespec64` helpers in `Threads.cpp` do. This is a host-safety fallback; the console's behaviour for an inaccessible time is not measured.
