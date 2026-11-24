# libkernel

## add Unknown function info

- [sigset and sigpause](../../../core/libs/prx/libkernel/System/src/Signals.cpp) (libkernel) - `sigset` follows POSIX for `SIG_HOLD`, with the value 3 from FreeBSD 11.4 [`sys/signal.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/sys/signal.h), and otherwise installs the disposition through `sigaction` with no flags, as FreeBSD `sigcompat.c` does (so no `SA_RESTART`), returning the previous one, or `SIG_HOLD` when the signal was blocked, which it then unblocks. `sigpause` takes a signal mask, as FreeBSD `sigcompat.c` does: it stores the argument in the first word of the set and calls `sigsuspend`, so it returns -1 with `EINTR` when a handler runs and restores the mask. The signal-number form, `xsi_sigpause`, is not exported. The console's `SIG_HOLD` value and the behaviour of both functions on the console are not measured. Both are obsolescent in POSIX.
