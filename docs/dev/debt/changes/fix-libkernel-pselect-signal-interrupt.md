# libkernel

## change [pselect](../../../core/libs/prx/libkernel/Socket/src/Pselect.cpp)

- [pselect](../../../core/libs/prx/libkernel/Socket/src/Pselect.cpp) (libkernel) now notices a caught signal that becomes pending while it is sleeping: the socket wait is retried in 1 ms slices, newly pending signals unblocked by the temporary mask are delivered between slices, and a caught handler makes the call return `EINTR` with its input descriptor sets preserved unless a descriptor became ready first. The 1 ms polling is a host approximation rather than FreeBSD's atomic signal-mask-and-sleep primitive, and the signal subsystem's existing delivery limits remain. Console timing and simultaneous signal/readiness precedence are unmeasured.