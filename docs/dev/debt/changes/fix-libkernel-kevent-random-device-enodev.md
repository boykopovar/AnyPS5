# libkernel

## add Functional

- [kevent on the random device](../../../core/libs/prx/libkernel/Equeue/src/Kqueue.cpp) (libkernel) - `EVFILT_READ` and `EVFILT_WRITE` on `/dev/random` and `/dev/urandom` fail with `ENODEV`, and their error event has `fflags` 0, as FreeBSD 11.4's [`random_cdevsw`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/dev/random/randomdev.c#L76-L83) has no `d_kqfilter`, which [`prep_cdevsw`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_conf.c#L689) sets to `enodev`. Not measured on a console. [sceKernelAddReadEvent and sceKernelAddWriteEvent](../../../core/libs/prx/libkernel/Equeue/Equeue.cpp) fail with `ENODEV` too. `EVFILT_PROCDESC` there throws (see the kevent measurements entry), and `EVFILT_VNODE` and `sceKernelAddFileEvent` watch the host device, where FreeBSD fails with `ENODEV` as well.
