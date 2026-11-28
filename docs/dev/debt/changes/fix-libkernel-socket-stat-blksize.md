# libkernel

## change [fstat on a guest socket]

- [fstat on a guest socket](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) fills the fields FreeBSD 11's soo_stat and pru_sense_null fill and zeroes the rest: S_IFSOCK, the read bits while the receive side is open or data is queued, the write bits while the send side is open, st_size and st_blksize. Only this process's shutdown closes a side here; a peer's shutdown, close or reset does not clear the bits as FreeBSD's socantrcvmore and socantsendmore do. st_size is the host's FIONREAD, which for a Linux datagram socket is the size of the next datagram where FreeBSD counts every queued byte. A socketpair end's st_ino comes from a counter this process starts at 1, where FreeBSD's uipc_sense uses one counter for every local socket on the system, so the numbers differ from the console's
