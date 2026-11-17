# libkernel

## change loses received stream data on an output copy fault

- [recvmsg](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) keeps stream data queued when the first byte of the output buffer cannot be written, as FreeBSD 11.4 does (on POSIX hosts not on a local socket offered a control buffer, which takes the descriptor-passing receive), but loses it when a later part of the buffer faults, as outputs are copied after the host receive, where FreeBSD keeps the data it could not copy; a datagram is dropped on a copy fault in both ([`soreceive_dgram`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/uipc_socket.c#L2308-L2312), [`soreceive_generic`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/uipc_socket.c#L1758-L1774)). A Linux `process_vm_readv` or `process_vm_writev` failure other than `EFAULT` throws
