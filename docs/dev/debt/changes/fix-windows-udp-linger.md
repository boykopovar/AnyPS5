# libkernel

## change [SO_LINGER on a datagram socket]

- [SO_LINGER on a datagram socket](../../../core/libs/prx/libkernel/Socket/src/SocketOptions.cpp) (libkernel) - on Windows an IPv4 or IPv6 UDP socket stores its own linger option instead of asking Winsock, which rejects it. Setting it stores the seconds even when disabled; a get returns the `SO_LINGER` bit or 0 and the stored seconds, as FreeBSD 11.4's [`sosetopt` and `sogetopt`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/uipc_socket.c#L2535-L2552) do. This models option storage and read-back, not a change to UDP close or delivery. `sceNetSetsockopt` and `sceNetGetsockopt` share this behavior. A PS5 UDP measurement is not available.
