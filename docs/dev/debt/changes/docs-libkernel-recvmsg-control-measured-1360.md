# libkernel

## change [SO_TIMESTAMP, SO_BINTIME](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel)

- [SO_TIMESTAMP, SO_BINTIME](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) - the arrival time in their control messages is `CLOCK_REALTIME` read when the host hands the datagram over, not when it reached the socket. Otherwise `recvmsg` on a UDP socket returns them as a payload process on PS5 firmware 13.60 got them: one 32-byte `SCM_BINTIME` (IPv4 only) then one `SCM_TIMESTAMP` at `SOL_SOCKET`, and a control buffer too small for them gets as many bytes as fit, its length, and `MSG_CTRUNC`. That partial copy is the console's: FreeBSD 11.4's `kern_recvit` copies whole messages only. A PS5 Pro payload process (13.60) settled the rest: an empty buffer with a real pointer gets nothing and `MSG_CTRUNC`, a null control pointer gets nothing without the flag, and TCP and Unix datagram sockets get no message at all; here it happens the same way
