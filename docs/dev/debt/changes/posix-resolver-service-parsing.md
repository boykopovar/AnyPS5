# libScePosixForWebKit

## add Functional

- [Resolver service parsing](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) is unverified on a console. Numeric service grammar and error classification follow FreeBSD 11.4's LP64 [str2number](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/getaddrinfo.c#L372-L387) and [get_port](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/getaddrinfo.c#L1484-L1581). Named services use the host service database; its contents and matching rules can differ from the console. Valid SCTP/UDPLite queries and address ordering remain host-dependent. The parser retains the bridge's errno preservation for non-`EAI_SYSTEM` results rather than reproducing FreeBSD's temporary `strtoul` errno changes. Closes when: compared with the console.
