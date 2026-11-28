# libc

## add Unknown function info

- [TimeExtras.cpp](../../../core/libs/prx/libc/src/TimeExtras.cpp): `gmtime_r`, `localtime_r`, `ctime_r`, `timegm`, `xtime_get`, and `Xtime` helpers copy guest inputs/outputs through `GuestArenaPrepareHostRead`, `GuestArena::HostWrite`, `GuestCopyIn`, and `GuestCopyOut`, returning `nullptr`/`-1` with `EFAULT`, `0`, or `std::invalid_argument` on inaccessible guest pointers because PS5 hardware fault behavior is unverified.
