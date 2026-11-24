# libSceRudp

## change [sceRudpProcessEvents, sceRudpNetFlush and sceRudpNetReceived]

- [sceRudpProcessEvents and sceRudpNetFlush](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) return success after initialization and `RUDP_ERROR_NOT_INITIALIZED` before it, following [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/rudp/rudp.cpp). The host does not queue or send RUDP packets. PPSA03490 calls `sceRudpProcessEvents(0)` followed by `sceRudpNetFlush()` from its P2P service thread and ignores the first result.

## add Silent stubs

- [sceRudpNetReceived](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) discards nonempty packets on a bound socket and returns 0 after the argument and socket checks. The [firmware 13.42 harness](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) returned `0x8077001b` for three malformed packets. General packet validation, delivery and the result for a well-formed packet remain unverified. An unreadable source address returns the host safety error `0x80770004`.
