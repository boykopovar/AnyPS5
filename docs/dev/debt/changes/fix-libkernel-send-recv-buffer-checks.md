# libkernel

## change [send and recv]

- [send, recv, sendto and recvfrom](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) - on Windows a stream receive whose writable prefix is shorter than the queued data fails with `EFAULT`, but data arriving after the check, or `MSG_WAITALL`, can return the prefix as a short success where FreeBSD fails. Linux and macOS return the bytes copied before a fault on a stream, where FreeBSD returns `EFAULT`. On macOS a datagram whose copy into a `recv` buffer faults stays queued, where `recvfrom` drops it as FreeBSD does.
