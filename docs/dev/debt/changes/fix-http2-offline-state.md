# libSceHttp2

## change [libSceHttp2 offline requests]

- [libSceHttp2 offline requests](../../../core/libs/prx/libSceHttp2/Export.cpp) - the console's offline send error is unmeasured. `APS5_NET_DISCONNECTED` records simulated error `0x80431063` without connecting, including before async completion. Failed-send queries follow [#17735](https://github.com/boykopovar/AnyPS5/pull/17735): status 0, unavailable length/read. Live async submissions still throw; their event, timeout and cancellation contracts remain unimplemented.
