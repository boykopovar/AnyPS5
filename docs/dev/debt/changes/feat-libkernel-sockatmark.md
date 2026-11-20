# libkernel

## add Functional

- [sockatmark](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) returns the host TCP socket's out-of-band mark through `SIOCATMARK`, including libSceNet sockets borrowed through the existing native-socket lookup. Datagram sockets return zero as FreeBSD 11.4's `sockatmark` does. The guest interface and invalid-descriptor errors are checked locally; console behavior, the provider of Azahar's unqualified import, and differences between host TCP urgent-data handling remain unverified.
