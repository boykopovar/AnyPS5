# libkernel

## add Functional

- [kevent changelist and error events](../../../core/libs/prx/libkernel/Equeue/src/Kqueue.cpp) (libkernel) follow FreeBSD 11.0's [`kqueue_kevent`](https://github.com/freebsd/freebsd-src/blob/release/11.0.0/sys/kern/kern_event.c) and are not measured on a console: the changelist is read in batches of 8 (`KQ_NEVENTS`), so a batch that cannot be read fails with `EFAULT` before any of its changes is applied, and an error event that cannot be written to the eventlist is dropped but still counted, without advancing the eventlist
