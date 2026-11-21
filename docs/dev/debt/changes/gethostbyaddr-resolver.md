# libScePosixForWebKit

## add Functional

- [gethostbyaddr](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) follows FreeBSD 11.4's [binary-address signature and host-entry result](https://man.freebsd.org/cgi/man.cgi?apropos=0&manpath=FreeBSD+11.4-RELEASE&query=gethostbyaddr&sektion=3) and its [IPv6 mapping and validation](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/net/gethostnamadr.c). IPv4 and IPv6 use the host resolver; its DNS policy and records can differ from the console. The returned `hostent` is copied into per-thread storage with guest address-family values. A link-local IPv6 address returns `HOST_NOT_FOUND`; mapped or compatible IPv4 uses an IPv4 query. Invalid family or length returns `NETDB_INTERNAL` with guest `EAFNOSUPPORT` or `EINVAL`; an unreadable address throws instead of faulting. The console provider, ABI and failure behavior are not measured. Closes when: compared with the console.
