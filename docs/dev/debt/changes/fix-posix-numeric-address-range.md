# libScePosixForWebKit

## change [gethostbyname]

- [gethostbyname](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) (libScePosixForWebKit) - numeric IPv4 forms and trailing whitespace follow FreeBSD 11.4 [inet_aton](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/inet/inet_addr.c). A numeric component above 0xFFFFFFFF is rejected instead of wrapping, which is a deliberate deviation from that reference: the FreeBSD comparison is not re-measured here, and the console's behavior for such values is not measured, so whether it rejects or wraps them is unverified. Clearing the resolver error on numeric success follows [fakeaddr](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libc/net/gethostnamadr.c). Exact PS5 behavior is unverified; hostname lookup still uses the host resolver. Closes when: measured on a PS5
