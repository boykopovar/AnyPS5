# libkernel

## change [EVFILT_TIMER modify with data 0]

- [EVFILT_TIMER modify with data 0](../../../core/libs/prx/libkernel/Equeue/src/Kqueue.cpp) (libkernel) - Zero-data changes with flags 0 or only `EV_CLEAR` keep the old period. FreeBSD 11.0 copies the new period data in those cases. Positive periods on enable and disable are unmeasured on console. One-shot re-adds, negative periods, unknown units, unsupported flags and periods that overflow the armed expiry throw. Zero-data re-add, enable and disable changes use 100 µs and keep the armed expiry. The [PS5 13.60 probe](https://gist.github.com/orezraey/c351d109ca00ab6cfc2721df8d076437#file-ptimer2-console-log) saw 13 and 10 expirations in events about 1 ms apart after a zero-data re-add, and 912 expirations about 92 ms after the deadline on enable. Timestamps are in whole milliseconds. Enable reports expirations at the period in effect while disabled.
