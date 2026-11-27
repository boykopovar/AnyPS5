# libkernel

## add Functional

- [accept peer-address length on forwarded sockets](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) - stages the full peer address separately from the caller's capacity, copies the smaller of that capacity and the address length, and reports the copied length. This follows [FreeBSD 11.4 kern_accept4 and accept1](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/uipc_syscalls.c#L278-L475), which clip the address length before copying it out. IPv4 and IPv6 regression tests cover zero, short, exact and oversized capacities for native and libSceNet-owned sockets; this length policy has not been measured on a PS5. Closes when: measured on a PS5. Labels: console, tested.
