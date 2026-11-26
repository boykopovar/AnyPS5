# libkernel

## add Functional

- [getsockopt LOCAL_PEERCRED, LOCAL_CREDS and LOCAL_CONNWAIT](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) - throw on an `AF_UNIX` socket received through `SCM_RIGHTS` or adopted from the host, whose peer credentials are not known, and on every `AF_UNIX` socket setting `LOCAL_CREDS` throws, as `recvmsg` adds no `SCM_CREDS` message, and so does setting `LOCAL_CONNWAIT`, as that flag is not kept. On a `socketpair` socket they follow FreeBSD 11.4's [`kern_socketpair`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/uipc_syscalls.c#L696-L705) and [`uipc_ctloutput`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/uipc_usrreq.c#L1222-L1268): a stream or sequenced-packet pair reports the `xucred` of the identity it was made with (the root identity of `geteuid` and the first 16 groups of `getgroups`), a datagram pair fails with `EINVAL`, and `LOCAL_CREDS` and `LOCAL_CONNWAIT` read 0
