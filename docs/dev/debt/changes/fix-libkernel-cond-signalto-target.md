# libkernel

## change [pthread_cond_signalto_np, scePthreadCondSignalto]

- [pthread_cond_signalto_np, scePthreadCondSignalto](../../../core/libs/prx/libkernel/Pthread/src/Cond.cpp) (libkernel) wake only the requested waiter, as a payload process on PS5 firmware 13.60 did for both exports: with two waiters, `signalto(A)` woke A while B timed out. Each condition wait registers its own host wait condition and a pending-wake flag before releasing the guest mutex, so a targeted signal that arrives before the host wait starts is retained instead of being lost. A handle that is not a thread returns `ESRCH`, a null thread returns `EINVAL`, and a live thread that is not waiting on that condition returns `EPERM`, matching that console measurement. A finished, unjoined thread returns `ESRCH` here but was not measured on the console; cancellation racing a targeted signal is also unverified.
