# libkernel_sys

## change [sched_rr_get_interval]

- [sched_rr_get_interval](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) reports 4 ms for pid 0, the host-backed guest pid and pid 1, as a PS5 payload process (firmware 11.60, probe `libkernel_sys_rrinterval_adjtime`, build `ace230ef0cab`) did through the libkernel export; pids 0x7fffffff and -1 return `ESRCH` with the interval untouched and a null interval returns `EFAULT`, as measured. Other pids return `ESRCH` here, which was not measured for pids that exist on the console. AnyPS5 does not run a guest scheduler or track a per-process configured quantum, so the 4 ms is the console's reported value, not runtime scheduling.
