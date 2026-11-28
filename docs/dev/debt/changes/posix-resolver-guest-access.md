# libScePosixForWebKit

## add Functional

- [Resolver guest access](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp), including its 1024-byte input-string limit, is unverified on a console. `getaddrinfo` returns its existing invalid-guest-pointer result `EAI_FAIL` for unreadable host and service strings or strings without a NUL in the first 1024 bytes. `gethostbyname` and its reentrant form throw `std::invalid_argument` for those strings and inaccessible outputs. Reentrant result pointers still address the caller's aligned buffer; staging that buffer can throw `std::bad_alloc`. Input strings are copied before host resolution, so concurrent modifications are not observed.

- [Numeric canonical names](../../../core/libs/prx/libScePosixForWebKit/Resolver.cpp) have no console verification. `getaddrinfo` preserves the supplied numeric host text for `AI_CANONNAME`, including IPv6 spelling and scope, following FreeBSD 11.4 [explore_numeric and explore_numeric_scope](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/getaddrinfo.c#L1242-L1390). Numeric address acceptance and scope parsing still depend on the host resolver.
