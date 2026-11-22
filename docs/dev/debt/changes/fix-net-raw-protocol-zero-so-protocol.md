# libSceNet

## change `SO_PROTOCOL` on raw sockets throws

- [sceNetGetsockopt](../../../core/libs/prx/libSceNet/Export.cpp) - `SO_PROTOCOL` (`0x1016`) and its truncated integer output are not measured on PS5. The values follow FreeBSD 11.4: [sogetopt](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/uipc_socket.c) returns the selected entry's `pr_protocol`, and for protocol 0 [pffindtype](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/uipc_domain.c) takes the type's first entry in [in_proto.c](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/netinet/in_proto.c) and [in6_proto.c](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/netinet6/in6_proto.c): TCP, UDP or `IPPROTO_RAW`. A raw socket with an unlisted nonzero protocol gets the wildcard's 0. A Linux host refuses a raw socket with protocol 0 ([inet_create](https://github.com/torvalds/linux/blob/v6.12/net/ipv4/af_inet.c#L275-L288)), which FreeBSD accepts; Windows is untested.
