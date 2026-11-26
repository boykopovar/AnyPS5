# libSceRudp

## change [sceRudpProcessEvents, sceRudpNetFlush and sceRudpNetReceived]

- [sceRudpProcessEvents, sceRudpNetFlush and sceRudpNetReceived](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) - there is no reliable-UDP stack on the host, and no context here holds pending data to process, send or receive. `sceRudpProcessEvents` and `sceRudpNetFlush` return success after `sceRudpInit` and `RUDP_ERROR_NOT_INITIALIZED` before it. A firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678) under an emulator with imports stubbed, not on a console, also gives 0 for both with nothing pending; `sceRudpProcessEvents` instead gives `0x80770010` when the internal I/O thread is enabled. The same reading says its nonzero timeout is an unsigned 64-bit microsecond wait; no pending work exists here, so the wait itself is not modelled. `sceRudpNetReceived` remains a host-side stub until its successful packet path is known.

## change [sceRudpEnableInternalIOThread2]

- [sceRudpEnableInternalIOThread and sceRudpEnableInternalIOThread2](../../../core/libs/prx/libSceRudp/Export.cpp) (libSceRudp) - follow the same firmware 13.42 [run](https://github.com/boykopovar/AnyPS5/issues/13682#issuecomment-6932414678), under an emulator with imports stubbed and not on a console: after initialization they return `0x80770011` while any context exists, before the already-enabled check, and otherwise the first call succeeds while a later call returns `0x80770010`. The stack size, priority and affinity are not rejected by the exports. Here that state and error order are modelled and do not survive `sceRudpEnd` followed by a new `sceRudpInit`, but no host I/O thread is started and the scheduling parameters and affinity are not applied.
