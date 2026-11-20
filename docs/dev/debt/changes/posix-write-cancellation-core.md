# libkernel

## add Functional

- [POSIX write cancellation](../../../core/libs/prx/libkernel/Pthread/src/NativeIo.cpp) (libkernel) is not console-verified. It covers `write`, `_write`, `writev`, `_writev`, `pwrite`, `pwritev`, `send`, `sendto`, `sendmsg`, `_sendto` and `_sendmsg` on accepted file, shared-memory, pipe and socket descriptors; positioned socket and pipe writes retain their existing errors. Completed positive results, including partial counts, win over pending cancellation; zero and failed writes check cancellation after guards release, following FreeBSD libthr. SCE writes and libSceNet gain no cancellation points. Non-interruptible drivers can delay cancellation and close indefinitely. Windows pipe quota waits retry readiness at 1 ms intervals. Descriptor replacement and stdio closure can still wait indefinitely; stream-socket writes share the read admission and close/drain machinery.

## add Functional

- [Synchronous POSIX write cancellation](../../../core/libs/prx/libkernel/File/src/DescriptorControl.cpp) (libkernel) has no console measurement of interrupted `O_FSYNC` writes. Synchronization is a separate native flush after buffered progress. If that flush is interrupted, the write retains its positive byte count, and cancellation remains pending for the next cancellation point; buffered bytes may be visible without durable completion. A guest signal does not turn those bytes into `EINTR` or replay them. Direct SCE writes retain their existing flush and cancellation behavior.
