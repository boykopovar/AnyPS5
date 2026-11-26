# libSceRudp

## change [sceRudpProcessEvents, sceRudpNetFlush and sceRudpNetReceived]

- [sceRudpProcessEvents and sceRudpNetFlush](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) - no reliable-UDP stack is implemented, and every `sceRudpWrite` and `sceRudpRead` on a known context rejects the transfer without queueing data. Both processing calls return success after initialization and `RUDP_ERROR_NOT_INITIALIZED` before it; `sceRudpProcessEvents` instead returns `0x80770010` when the internal I/O thread is enabled. A firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) under an emulator with imports stubbed, not on a console, confirms the idle results and internal-thread error. Its nonzero timeout is an unsigned 64-bit microsecond wait; with no pending work here, the wait is not modelled.

## change [sceRudpEnableInternalIOThread2]

- [sceRudpEnableInternalIOThread and sceRudpEnableInternalIOThread2](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) - follow the same firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678), under an emulator with imports stubbed and not on a console: after initialization they return `0x80770011` while any context exists, before the already-enabled check, and otherwise the first call succeeds while a later call returns `0x80770010`. The stack size, priority and affinity are not rejected by the exports. Here that state and error order are modelled and do not survive `sceRudpEnd` followed by a new `sceRudpInit`, but no host I/O thread is started and the scheduling parameters and affinity are not applied.

## add Functional

- [sceRudpWrite and sceRudpRead](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) follow @BunnyFl's firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) under an emulator with imports stubbed, not console hardware. After initialization, `Write` rejects a negative id, null data or zero length; `Read` rejects a negative id, null data or `status[0]` above `0x0c`, while allowing zero length and null status. An unknown id gives `0x80770003`; every created, bound, listening or activated state reached gives `0x80770011`. `Read` still copies `status[0]` bytes back on that error; observed copied bytes kept the size byte and zeroed the rest. No run connected to a peer, so successful transfer stays unsupported. Inaccessible guest buffers use the invalid-argument host fallback, unmeasured on console hardware.

## add Silent stubs

- [sceRudpNetReceived](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) discards nonempty packets on a bound socket and returns 0 after the argument and socket checks. The [firmware 13.42 harness](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) returned `0x8077001b` for three malformed packets. General packet validation, delivery and the result for a well-formed packet remain unverified. An unreadable source address returns the host safety error `0x80770004`.
