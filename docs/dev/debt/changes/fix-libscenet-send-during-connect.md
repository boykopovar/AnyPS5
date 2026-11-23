# libSceNet

## add Functional

- [sceNetSend, sceNetSendto and sceNetSendmsg](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) without an address on a stream socket whose `sceNetConnect` is still in progress fail with `ENOTCONN` on Linux while the host socket is not writable, as FreeBSD 11.4's [`sosend_generic`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/uipc_socket.c#L1260-L1292) does for a socket that is not connected; the Linux host would wait for the handshake or fail with `EAGAIN`. A connect that ended at its `SO_CONNECTTIMEO` limit is not marked. On Windows and macOS such a send still goes to the host. Not measured on a console.
