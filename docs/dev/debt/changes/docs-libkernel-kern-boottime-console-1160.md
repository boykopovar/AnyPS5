# libkernel

## change [kern.boottime]

- [kern.boottime](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - `sysctl` {`CTL_KERN`, `KERN_BOOTTIME`} and `sysctlbyname` report a read-only 16-byte `struct timeval`, as FreeBSD 11.4's [`sysctl_kern_boottime`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_tc.c#L144-L163) does: the guest's `CLOCK_REALTIME` minus its `CLOCK_UPTIME`, taken at the first read and kept, so a later step of the host clock does not move it as FreeBSD's `tc_setclock` would. A payload process on PS5 firmware 13.60 and 11.60 gets the same value, `CLOCK_REALTIME` minus `CLOCK_UPTIME`, unchanged 2 s later, and an 8-byte buffer gets the first 8 bytes and `ENOMEM`; what a title process gets has not been measured.
