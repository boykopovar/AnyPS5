# libkernel

## remove [setitimer](

## add Functional

- [setitimer](../../../core/libs/prx/libkernel/System/src/Signals.cpp) (libkernel) - follows FreeBSD's `sys_setitimer` and `kern_setitimer`: `ITIMER_REAL` keeps its wall-clock timer behavior, while `ITIMER_VIRTUAL` uses the existing guest process user-CPU clock and expires as `SIGVTALRM`, and `ITIMER_PROF` uses the existing guest process user-plus-system CPU clock and expires as `SIGPROF`. CPU timers return their actual interval and remaining CPU time from `getitimer`/the null-value `setitimer` read path, return previous state when re-armed or disarmed, and re-arm periodic expiries by `it_interval`. Non-zero CPU-timer values and intervals below 1 ms are rounded up to 1 ms, matching FreeBSD's minimum-tick behavior at the resolution exposed by the guest CPU clocks. With the timer signal blocked on the arming thread, expiry is left process-pending for `sigwait`, `sigtimedwait` or `sigwaitinfo`; `SIG_IGN` drops it and the default disposition terminates with status 154 (`SIGVTALRM`) or 155 (`SIGPROF`). An unblocked guest handler still throws because guest handlers are not run from the host timer thread. CPU expiry is detected by a host poller at 1 ms wall-clock cadence rather than by FreeBSD scheduler ticks, and the console's exact `hz` and CPU-timer tick behavior have not been measured.
