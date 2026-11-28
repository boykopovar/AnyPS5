# libkernel

## change [getdtablesize](../../../core/libs/prx/libkernel/System/src/Process.cpp)

- [getdtablesize](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - returns the smaller of the `RLIMIT_NOFILE` soft limit, the host's `getdtablesize()` and 12555: FreeBSD returns the smaller of the soft limit and `maxfilesperproc` ([`sys_getdtablesize`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/kern_descrip.c#L338-L355)), and a payload process on PS5 firmware 11.60 returned 12555 for a soft limit of 13952, 64 for 64, and 12555 once both limits were clamped to 12555. `sysctlbyname` of `kern.maxfilesperproc` and `kern.maxfiles` failed with `ENOENT`, so 12555 rests on those three measurements rather than a read. A title process may differ. On Linux and macOS the guest limit is the host's soft limit. VLC (PPSA85300) imports it for a `closefrom` loop that never runs because its `fork` fails with `ENOSYS`
