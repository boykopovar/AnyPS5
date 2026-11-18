# libkernel

## change [socketpair]

- [socketpair](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) - the pair is the host's `AF_UNIX` pair on Linux and macOS and, on Windows, a loopback TCP connection for `SOCK_STREAM` or two connected UDP sockets for `SOCK_DGRAM`; `SOCK_SEQPACKET` throws on Windows and macOS, which has none. A datagram sent to a full receiver is dropped on Windows, where the console fails it with `ENOBUFS` at once. `AF_UNIX` paths are not resolved: `bind`, and a `connect` or addressed `sendto` that FreeBSD 11.4 passes to `unp_connect`, report `EAFNOSUPPORT`, and `connect` on a datagram end keeps it connected where FreeBSD disconnects it first. On Windows `poll` on a stream end reports no `POLLHUP` after the peer's `close`, where `unp_drop` makes the console report `POLLIN` with `POLLHUP`: the loopback connection cannot tell a `close` from a `shutdown(SHUT_WR)`
