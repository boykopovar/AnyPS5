# libkernel

## change [clock_settime]

- [clock_settime](../../../core/libs/prx/libkernel/Time/Time.cpp) (libkernel) - a PS5 Pro payload process (firmware 13.60) accepted ids 16 and 17 without moving realtime; which clock they hold is unknown, here they change nothing. A null time pointer fails with `EINVAL`, an unreadable one with `EFAULT`; nanoseconds outside 0 to 999999999 or a negative second fail with `EINVAL`, seconds have no upper bound (`INT64_MAX` was stored raw); every other clock id answers `EINVAL`. `CLOCK_REALTIME` now sets the host wall clock, which needs host privilege: without it the call fails with `EPERM`, and seconds beyond what the host represents fail where the console accepted them. The jail model of #11216 does not hold; a title process was not measured.

## change [settimeofday]

- [settimeofday](../../../core/libs/prx/libkernel/Time/Time.cpp) (libkernel_sys) - a PS5 Pro payload process (firmware 13.60) accepted null arguments and set the wall clock with a valid time, so the jail model of #11216 does not hold. It copies in the time (`EFAULT` for an unreadable pointer), rejects microseconds outside 0 to 999999 or negative seconds with `EINVAL`, and forwards realtime to the host clock; without host privilege that fails with `EPERM`. A non-null timezone was not measured on a console: it still copies in and fails with `EPERM`, storing nothing, and `gettimeofday` reports 0 and 0. A title process was not measured.
