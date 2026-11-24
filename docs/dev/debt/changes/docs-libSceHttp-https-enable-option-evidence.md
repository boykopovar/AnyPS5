# libSceHttp

## remove [sceHttpsEnableOption, sceHttpsDisableOption]

## change [sceHttpAbortRequest]

- [sceHttpAbortRequest](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for every request ID, before `sceHttpInit` and for an unknown ID too; no request reaches the network, so nothing is aborted. `sceHttpAbortRequestForce` calls it. [GuestHttpOptions.cpp](../../../core/libs/tests/GuestHttpOptions.cpp) tests only a live request, and the commit that added it names no measurement. shadPS4 (PS4, `src/core/libraries/network/http.cpp`, commit 0614ce324be092791b758d79f93d9f1ca5b10a81) returns `BEFORE_INIT` without a context and `INVALID_ID` for an unknown request, and marks a request that is created or sending as aborted; KytyPS5 (PS5, `src/libs/network.cpp`, commit 5e837a9beac82c031868eaad9348d6a48245fb99) returns `INVALID_ID` for an unknown request and otherwise OK without aborting; fpPS4 (PS4, `src/ps4_libscehttp.pas`, commit 04cefd43e6fddd1ab033e7980cd356d14c964905) always returns 0. PPSA85300 imports it (NID `hvG6GfBMXg8`, 1 ref); the console exports it.

## add Silent stubs

- [sceHttpsEnableOption, sceHttpsDisableOption](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the public TLS option toggles return 0 for any id and keep nothing, since no request reaches the network. Unlike the `*Private` variants above, they do not reject flags outside the `0x2dff` mask: the `ssl_flags` argument is ignored outright. PPSA99666 (DOOM) imports `sceHttpsEnableOption` (NID `f42K37mm5RM`, one reference), and the console's `libSceHttp` exports that NID; no local title imports `sceHttpsDisableOption`.
