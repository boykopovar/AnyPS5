# libSceHttp2

## add Functional

- [libSceHttp2 offline declared length](../../../core/libs/prx/libSceHttp2/Transport.cpp) - whether the console checks the body size against the declared content length before it fails an offline send is unmeasured. With `APS5_NET_DISCONNECTED` set, a send whose size differs from the declared length fails with `0x80431063` like any offline send; online, the mismatch still throws. PPSA17221 submits `sceHttp2SendRequestAsync` with size 0 and a declared length.
