# libSceNet

## add Functional

- [sceNetSendto and sceNetSendmsg](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) with an address on a connected datagram socket fail with `EISCONN` after checking that the data can be read (`EFAULT` first), as FreeBSD 11.4's [`udp_output`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/netinet/udp_usrreq.c#L1292-L1297) and [`udp6_output`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/netinet6/udp6_usrreq.c#L707-L716) do, where Linux and Windows send to the named address. An `AF_INET6` destination with port 0 still goes to the host, which `udp6_output` refuses first with `EADDRNOTAVAIL`. Not measured on a console.
