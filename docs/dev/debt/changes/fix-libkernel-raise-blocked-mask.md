# libkernel

## change does not consult the target thread's signal mask

- [sceKernelRaiseException](../../../core/libs/prx/libkernel/System/src/Exception.cpp) (libkernel) leaves the raise pending on a target thread that blocks the signal, as FreeBSD's `tdsendsignal` does for a signal sent to one thread, and runs the handler on that thread when it unblocks the signal through `sigprocmask`, `_sigprocmask` or `pthread_sigmask`, once for any number of raises made meanwhile. The end of a running handler's hold on the signal does not deliver it; the next mask change does. The console's behaviour is not verified
