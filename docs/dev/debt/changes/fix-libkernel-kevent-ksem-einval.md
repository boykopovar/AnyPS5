# libkernel

## add Functional

- [kevent on a ksem id](../../../core/libs/prx/libkernel/Equeue/src/Kqueue.cpp) (libkernel) - `EVFILT_READ` and `EVFILT_WRITE` on a ksem id fail with `EINVAL`, and their error event has `fflags` 0, as FreeBSD 11.4's `ksem_ops` attaches through [`invfo_kqfilter`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_descrip.c#L4060-L4065). [sceKernelAddReadEvent and sceKernelAddWriteEvent](../../../core/libs/prx/libkernel/Equeue/Equeue.cpp) on it still throw, and `EVFILT_VNODE` and `sceKernelAddFileEvent` watch the host null device behind the id, where FreeBSD fails with `EINVAL` too.
