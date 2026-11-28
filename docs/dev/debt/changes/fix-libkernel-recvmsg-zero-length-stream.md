# libkernel

## change [recv and recvfrom]

- [recv, recvfrom and recvmsg](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) answer some cases without the host. A zero-length receive on a stream socket returns 0, or `ENOTCONN` when it is not connected ([L1566-L1578](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/uipc_socket.c#L1566-L1578)), as on a PS5 (firmware 13.60). On an empty datagram socket it returns 0 at once; the console was measured only with `MSG_DONTWAIT`, and the blocking case follows FreeBSD 11.4's `soreceive_dgram`, which returns 0 before waiting. A zero-length `recvmsg` on a Unix stream socket with a control buffer still asks the host, as FreeBSD then delivers queued control messages, and the answers ignore a pending socket error, which FreeBSD returns first. A datagram another thread takes after the check makes a zero-length receive wait
