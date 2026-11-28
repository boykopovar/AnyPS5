# libkernel

## add Functional

- [SA_SIGINFO SIGFPE fault handlers](../../../core/libs/prx/libkernel/System/src/Exception.cpp) (libkernel) - an x87 or SIMD floating-point exception in guest code does not run an `SA_SIGINFO` handler on `SIGFPE`: it still ends the process (on macOS a `#DE` does too), or on Linux goes to the chained host handler; only a divide error (`#DE`, trapno 18) runs it, with `FPE_INTDIV`, under the conditions of `SA_SIGINFO fault handlers`. As for `SIGSEGV`, once `SIGFPE` gets another disposition on Linux, the host fault handler is not installed again and a later `SA_SIGINFO` handler does not run, even for `#DE`. On Linux, an `SA_SIGINFO` action on `SIGFPE` replacing a handler without `SA_SIGINFO` leaves the host signal with that handler's dispatcher, so the fault repeats forever instead of ending the process.
