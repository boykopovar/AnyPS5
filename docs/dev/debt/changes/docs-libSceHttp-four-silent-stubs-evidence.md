# libSceHttp

## remove [sceHttpsEnableOption, sceHttpsDisableOption]

## remove [sceHttpSetConnectTimeOut]

## remove [sceHttpSetResolveTimeOut]

## remove [sceHttpAddRequestHeader]

## add Silent stubs

- [sceHttpAddRequestHeader](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for any id and keeps nothing: the header `name`, `value` and `mode` are all ignored, so a request's caller-added headers are silently dropped (the counterpart of `sceHttpRemoveRequestHeader` above). No request reaches the network. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `EY28T2bkN7k`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpSetConnectTimeOut](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for any id and keeps the microsecond connection timeout nowhere, since no connection is made, as the send and receive timeout setters do. Call of Duty: Black Ops II (PPSA34502) imports it; PPSA85300 (VLC) and PPSA99666 (DOOM) import it too (NID `0S9tTH0uqTU`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpSetResolveTimeOut](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for any id and keeps the microsecond DNS-resolve timeout nowhere, the counterpart of `sceHttpSetResolveRetry` above; no name is resolved, since no request reaches the network. PPSA85300 (VLC) and PPSA99666 (DOOM) import it (NID `Tc-hAYDKtQc`, one reference each), and the console's `libSceHttp` exports that NID.
- [sceHttpsEnableOption, sceHttpsDisableOption](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the public TLS option toggles return 0 for any id and keep nothing, since no request reaches the network. Unlike the `*Private` variants above, they do not reject flags outside the `0x2dff` mask: the `ssl_flags` argument is ignored outright. PPSA99666 (DOOM) imports `sceHttpsEnableOption` (NID `f42K37mm5RM`, one reference), and the console's `libSceHttp` exports that NID; no local title imports `sceHttpsDisableOption`.
