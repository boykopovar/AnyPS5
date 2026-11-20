# libSceSsl

## add Functional

- [Retained libkernel transport waits](../../../core/libs/prx/libSceSsl/Export.cpp) (libSceSsl) can continue after guest close or cancellation on Unix. See [libkernel debt](libkernel.md) for the uncancellable readiness wait. Console behavior is unverified.

# libkernel

## add Functional

- [Retained native socket transport](../../../core/libs/prx/libkernel/Socket/src/Udp.cpp) (libkernel) can leave a Unix SSL transfer without a socket timeout waiting indefinitely after guest close or cancellation. The callback holds the socket owner across close and waits in host `poll` outside `ThreadCancel::Native` until peer progress or shutdown wakes it. Console behavior is unverified.
