# libSceHttp

## change has no response header to limit

- [libSceHttp](../../../core/libs/prx/libSceHttp/Export.cpp) - no request reaches the network, so `sceHttpRedirectCacheFlush` has no redirect to forget; `sceHttpsUnloadCert` returns success like `sceHttpsLoadCert`, which keeps no certificate; `sceHttpSetRedirectCallback` and `sceHttpSetCookieRecvCallback` accept callbacks that are never called, `sceHttpsSetSslVersion` has no TLS connection to configure and `sceHttpsGetSslError` reports error 0 with detail 0; `sceHttpSetAuthInfoCallback` accepts a callback that is never called, as no server asks for authentication; `sceHttpsGetCaList` and `sceHttpsFreeCaList` report an empty CA list (`certs` null, `certsNum` 0) and return 0, as no CA store is kept, and a null list returns `ERROR_INVALID_VALUE`; unlike shadPS4 they do not check that `http_ctx_id` is a live context nor that the library is initialised, and `sceHttpsFreeCaList` also nulls `certs`, which shadPS4 leaves untouched. The signatures and the `{ void* certs; int certsNum; }` layout follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/network/http.h) (PS4), not confirmed on a PS5 title

## remove [sceHttpSetSendTimeOut, sceHttpSetRecvTimeOut, sceHttpSetRequestContentLength]

## change [sceHttpRemoveRequestHeader, sceHttpSetResolveRetry, sceHttpSetChunkedTransferEnabled, sceHttpsSetMinSslVersion]

- [sceHttpRemoveRequestHeader, sceHttpSetChunkedTransferEnabled, sceHttpsSetMinSslVersion](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - return 0 and keep nothing, for any ID, since no request reaches the network. Of the 38 titles relinked here, only Call of Duty: Black Ops II (PPSA34502) imports one of them, `sceHttpSetChunkedTransferEnabled`

## change [sceHttpSetConnectTimeOut]

- [sceHttpSetConnectTimeOut](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for any id and keeps the microsecond connection timeout nowhere, since no connection is made. Unlike the other timeout setters it still does not check initialisation or its id. Call of Duty: Black Ops II (PPSA34502) imports it.

## remove [sceHttpSetResolveTimeOut]

## add Unknown function info

- [sceHttpSetSendTimeOut, sceHttpSetRecvTimeOut, sceHttpSetResolveTimeOut, sceHttpSetResolveRetry, sceHttpSetRecvBlockSize, sceHttpSetResponseHeaderMaxSize, sceHttpSetRequestContentLength](../../../core/libs/prx/libSceHttp/src/Options.cpp) (libSceHttp) - the order of checks, object kinds and value checks follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/264d6b71d8671a429ad2c9d1a281e18558fb9693/src/core/libraries/network/http.cpp) (PS4), not measured on a PS5; the firmware 13.42 disassembly covers only `sceHttpSetHttp09Enabled`, `sceHttpSetPriorityOption` and `sceHttpSetPolicyOption`. A negative retry, and a resolve timeout of 999999 us or less at SDK 1.70 or later, return `ERROR_INVALID_VALUE` before the id check; the content length takes only a request. Values start at 0 (unmeasured) and nothing reads them. The busy error for a request already sent is not modelled, and the length given to `sceHttpCreateRequest` is ignored
