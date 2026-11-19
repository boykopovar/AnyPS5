# libSceNpWebApi2

## change [sceNpWebApi2AddWebTraceTag]

- [sceNpWebApi2AddWebTraceTag](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - validates `value`, checks the packed library/user/request hierarchy (`0x80553404`/`0x80553405`/`0x80553406`), and stores `"X-Psn-WebTrace-Tag"` on the request, matching `libSceNpWebApi2.sprx` (firmware 13.60, words `0x13590001`/`0x13600007`, read from disassembly, not measured on a console); no request reaches a server (`sceNpWebApi2SendRequest` returns `UNAVAILABLE`), and already-sent header rejection (`0x8055341A`) is unmodeled.

## change [sceNpWebApi2DeleteUserContext,

- [sceNpWebApi2PushEventDeleteFilter, sceNpWebApi2PushEventDeletePushContext, sceNpWebApi2PushEventUnregisterCallback, sceNpWebApi2PushEventUnregisterPushContextCallback and sceNpWebApi2PushEventStartPushContextCallback](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - return 0 for any id without storing push filters, contexts, or callbacks. `sceNpWebApi2DeleteUserContext` now validates the packed context hierarchy (`0x80553404`/`0x80553405`) and destroys owned requests per firmware 13.60 `libSceNpWebApi2.sprx` (read from disassembly, not measured on a console). `sceNpWebApi2CreateUserContext` omits `libSceUserService` user-ID validation and `sceNpWebApi2DeleteUserContext` assumes immediate teardown with no in-flight request drain.

## change [sceNpWebApi2SetRequestTimeout]

- [sceNpWebApi2SetRequestTimeout](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - `sceNpWebApi2SetRequestTimeout`, `sceNpWebApi2AbortRequest`, `sceNpWebApi2AddHttpRequestHeader`, and `sceNpWebApi2DeleteRequest` now check the 3-tier packed ID hierarchy (`0x80553404`/`0x80553405`/`0x80553406`) and record timeout, abort, and header state per firmware 13.60 `libSceNpWebApi2.sprx` (read from disassembly, not measured on a console); offline requests never reach a network (`sceNpWebApi2SendRequest` returns `UNAVAILABLE`), and already-sent header rejection (`0x8055341A`) or active-request abort waiting is unmodeled.

## change [sceNpWebApi2SetMultipartContentType and sceNpWebApi2AddMultipartPart]

- [sceNpWebApi2SetMultipartContentType and sceNpWebApi2AddMultipartPart](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - check the 3-tier packed ID hierarchy (`0x80553404`/`0x80553405`/`0x80553406`) and reject non-multipart requests with `0x80553414`, matching firmware 13.60 `libSceNpWebApi2.sprx` (read from disassembly, not measured on a console); `DeleteRequest`, `DeleteUserContext`, and `Terminate` clear owned requests. Host-safe guest reads/writes and throwing on `AddMultipartPart` header arrays remain, and `SendMultipartRequest` still returns `UNAVAILABLE`.

## change [sceNpWebApi2CreateMultipartRequest and sceNpWebApi2SendMultipartRequest]

- [sceNpWebApi2CreateMultipartRequest and sceNpWebApi2SendMultipartRequest](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - `sceNpWebApi2CreateMultipartRequest` validates null strings, looks up the packed library/user context (`0x80553404`/`0x80553405`), and allocates a packed multipart request ID per firmware 13.60 `libSceNpWebApi2.sprx` (read from disassembly, not measured on a console); `sceNpWebApi2SendMultipartRequest` still validates arguments and returns `SCE_NP_WEBAPI2_ERROR_UNAVAILABLE` without transmitting parts.
