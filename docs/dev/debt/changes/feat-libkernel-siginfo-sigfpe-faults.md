# libkernel

## add Functional

- [SA_SIGINFO SIGFPE fault handlers](../../../core/libs/prx/libkernel/System/src/Exception.cpp) (libkernel) - an x87 or SIMD floating-point exception in guest code does not run an `SA_SIGINFO` handler on `SIGFPE`: it still ends the process (on macOS a `#DE` does too), or on Linux goes to the host `SIGFPE` handler that was set when the host fault handler was first installed. That is the guest dispatcher if a `SIGFPE` handler without `SA_SIGINFO` was set before then, and with no such handler left it returns, so the fault repeats forever. Only a divide error (`#DE`, trapno 18) runs it, with `FPE_INTDIV`, under the conditions of `SA_SIGINFO fault handlers`.
