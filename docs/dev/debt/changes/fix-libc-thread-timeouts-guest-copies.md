# libc

## add Functional

- [_Thrd_join and std::__sce_v2::_Thrd_join](../../../core/libs/prx/libc/src/Threads.cpp) (libc) - a non-null exit-code output that cannot be written returns `thrd_error` (4) before joining, so the thread stays joinable. An `Xtime` argument of `_Cnd_timedwait`, `_Thrd_sleep` or the `__sce_v2` `Timespec64` helpers that cannot be read or written throws `std::invalid_argument`, as a null one does. Both are host-safety fallbacks: the console's behaviour for an inaccessible pointer is not measured.
