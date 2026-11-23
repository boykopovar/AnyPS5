# libkernel

## add Unknown function info

- [sigset and sigpause](../../../core/libs/prx/libkernel/System/src/Signals.cpp) (libkernel) - `sigset` follows POSIX for `SIG_HOLD`, with the value 3 from FreeBSD 11.4 [`sys/signal.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/sys/signal.h), and otherwise installs the disposition through `signal`, returning the previous one, or `SIG_HOLD` when the signal was blocked, which it then unblocks. `sigpause` unblocks the signal and calls `sigsuspend` with the rest of the mask, so it returns -1 with `EINTR` when a handler runs. The console's `SIG_HOLD` value and the behaviour of both functions on the console are not measured. Both are obsolescent in POSIX.
