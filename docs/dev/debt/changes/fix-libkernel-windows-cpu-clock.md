# libkernel

## add Functional

- Thread CPU time on Windows ([clock_gettime](../../../core/libs/prx/libkernel/Time/Time.cpp) with the thread CPU clock, and the [CPU time of a guest thread](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp)) is measured only on one AMD CPU with an invariant TSC: `QueryThreadCycleTime`, for which Microsoft documents no time unit, divided by the TSC frequency libkernel measures. On a Ryzen 5 7600X it stayed within 0.25% of wall time without system calls; each read of the clock went about 6.5 ns uncounted, other system calls 1 ns or less. The first read, if the TSC frequency is not measured yet, pins the thread to one CPU and waits 20 ms. Without an invariant TSC these clocks read `GetThreadTimes`, which moves in 15.6 ms steps, as the process CPU clocks and `getrusage` still do; a thread clock and those can differ by about one step. Closes when: measured on an Intel CPU and on a CPU without an invariant TSC.
