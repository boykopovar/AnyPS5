# libSceRudp

## change [sceRudpProcessEvents, sceRudpNetFlush and sceRudpNetReceived]

- [sceRudpProcessEvents and sceRudpNetFlush](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) - there is no reliable-UDP stack on the host, and every `sceRudpWrite` and `sceRudpRead` call on a known context here reports `RUDP_ERROR_NOT_ACCEPTABLE` without queueing data while `sceRudpGetStatus` reports an idle context, so no context ever holds pending data to process or send; the two calls return success after `sceRudpInit` and `RUDP_ERROR_NOT_INITIALIZED` before it, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/rudp/rudp.cpp) returns OK from its stubs. PPSA03490 calls `sceRudpProcessEvents(0)` followed by `sceRudpNetFlush()` from its P2P service thread and ignores the first result.

## add Functional

- [sceRudpWrite and sceRudpRead](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) follow @BunnyFl's firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) under an emulator with imports stubbed, not console hardware. After initialization, `Write` rejects a negative id, null data or zero length; `Read` rejects a negative id, null data or `status[0]` above `0x0c`, while allowing zero length and null status. An unknown id gives `0x80770003`; every created, bound, listening or activated state reached gives `0x80770011`. `Read` still copies `status[0]` bytes back on that error; observed copied bytes kept the size byte and zeroed the rest. No run connected to a peer, so successful transfer stays unsupported. Inaccessible guest buffers use the invalid-argument host fallback, unmeasured on console hardware.

## add Silent stubs

- [sceRudpNetReceived](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) discards nonempty packets on a bound socket and returns 0 after the argument and socket checks. The [firmware 13.42 harness](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) returned `0x8077001b` for three malformed packets. General packet validation, delivery and the result for a well-formed packet remain unverified. An unreadable source address returns the host safety error `0x80770004`.
