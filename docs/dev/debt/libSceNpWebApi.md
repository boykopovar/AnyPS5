# libSceNpWebApi technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceNpWebApi/InventoryExports.cpp) - 38 NIDs occur as functions under the same module and logical library in both version-labelled images. The labels do not independently authenticate retail firmware provenance; the NIDs are absent from the pinned public 3.20 catalogue. Names, prototypes and behavior are unverified. These import-resolution declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| Unknown | `+aMuhoVidDY` |
| Unknown | `0cCtt7Uv6rU` |
| Unknown | `4yR2XRjuTRI` |
| Unknown | `54n5gNkHtlM` |
| Unknown | `B4OVXU6VY9o` |
| Unknown | `BkxO0e2+ueg` |
| Unknown | `Gm138-2DI6g` |
| Unknown | `HgaTom-g+VQ` |
| Unknown | `JKm18ddwAM8` |
| Unknown | `JNiFPWtH-Hk` |
| Unknown | `KEYeKen41pc` |
| Unknown | `PCliRwT6ueA` |
| Unknown | `PwJ4BO0uwR4` |
| Unknown | `QGbJTngpl80` |
| Unknown | `U2KAvj2rtSE` |
| Unknown | `V6DhvHJCGfM` |
| Unknown | `WBl0nAQLZjc` |
| Unknown | `YZjQyCXoYxk` |
| Unknown | `YfK56KsJN0M` |
| Unknown | `a8OI5hE-DUQ` |
| Unknown | `daA4FMfpA58` |
| Unknown | `eJ1gJsUhQW4` |
| Unknown | `fe1j0GOZ7-8` |
| Unknown | `flWi3MA9OVo` |
| Unknown | `fwS31KfUHoA` |
| Unknown | `jhZyUt+lyVc` |
| Unknown | `ldAEblBOOwk` |
| Unknown | `lyhL-aTxj98` |
| Unknown | `meMsH0c36rQ` |
| Unknown | `nP9mHqC8v4M` |
| Unknown | `nrDh9GesOyk` |
| Unknown | `ojGP5vur+qM` |
| Unknown | `ugei4b97OXE` |
| Unknown | `vQgD7uDMKaA` |
| Unknown | `vm9OVSS7E18` |
| Unknown | `wNSQ60gepNA` |
| Unknown | `wXXTksptCEo` |
| Unknown | `zQE2rxZdLy8` |

### Unknown function info

- [sceNpWebApiGetHttpStatusCode, sceNpWebApiReadData](../../../core/libs/prx/libSceNpWebApi/Export.cpp) - throw through `NotImplemented_nid_no_patch`; behaviour and the PS5 prototypes are unmeasured. The only source of the parameter lists is shadPS4's PS4-derived prototypes (GPLv2, used as a reference only), with no second reference; structure pointers are `void*` because no layout is known.
- [sceNpWebApiInitialize, sceNpWebApiTerminate, sceNpWebApiCreateHandle, sceNpWebApiDeleteHandle, sceNpWebApiCreatePushEventFilter, sceNpWebApiDeletePushEventFilter](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi, PS4) - signatures, id ranges (library contexts 1 to 0x7fff, handles and filters below 0xf0000000) and errors (`LIB_CONTEXT_NOT_FOUND`, `HANDLE_NOT_FOUND`, `PUSH_EVENT_FILTER_NOT_FOUND`, `INVALID_ARGUMENT` for a missing filter parameter) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_web_api/np_web_api_internal.cpp); they are not checked on a console. The libHttp context and pool size are kept but not used, since no request is ever sent. Limbo (CUSA01664) creates a context, a handle and filters when its game starts.

- [The firmware inventory exports listed below](../../../core/libs/prx/libSceNpWebApi/Export.cpp) have unknown signatures and behaviour. Only their names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscenpwebapi-30b6435a6f.jsonl). Their parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and every call throws through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `sceNpWebApiAddMultipartPart` | `19KgfJXgM+U` |
| `sceNpWebApiClearAllUnusedConnection` | `KQIkDGf80PQ` |
| `sceNpWebApiClearUnusedConnection` | `f-pgaNSd1zc` |
| `sceNpWebApiGetConnectionStats` | `UJ8H+7kVQUE` |
| `sceNpWebApiGetErrorCode` | `2qSZ0DgwTsc` |
| `sceNpWebApiIntCreateCtxIndExtdPushEventFilter` | `c1pKoztonB8` |
| `sceNpWebApiIntCreateServicePushEventFilter` | `TZSep4xB4EY` |
| `sceNpWebApiIntRegisterServicePushEventCallback` | `VjVukb2EWPc` |
| `sceNpWebApiIntRegisterServicePushEventCallbackA` | `sfq23ZVHVEw` |
| `sceNpWebApiRegisterExtdPushEventCallback` | `vrM02A5Gy1M` |
| `sceNpWebApiSendMultipartRequest` | `KCItz6QkeGs` |
| `sceNpWebApiSendMultipartRequest2` | `DsPOTEvSe7M` |
| `sceNpWebApiSetMaxConnection` | `gRiilVCvfAI` |

- [sceNpWebApiAbortHandle, sceNpWebApiAddHttpRequestHeader, sceNpWebApiCheckTimeout, sceNpWebApiCreateMultipartRequest, sceNpWebApiSendRequest2, sceNpWebApiSetHandleTimeout, sceNpWebApiSetMultipartContentType, sceNpWebApiSetRequestTimeout](../../../core/libs/prx/libSceNpWebApi/Export.cpp) - the timeout unit is unverified: [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/1d8004a22f/src/core/libraries/np/np_web_api/np_web_api_internal.cpp) logs both timeouts as ms but adds them to microsecond process time, as done here from a request's first send. Not checked on a console; prototypes are [Orbital](https://github.com/AlexAltea/orbital/blob/b321fcdbd7/tools/ida/db_types.json#L1665-L1696)'s, checks shadPS4's. Headers, multipart types and handle timeouts go unused, as nothing is sent. `sceNpWebApiCheckTimeout` aborts expired requests at most once per 1000 µs, then a send returns `NOT_SIGNED_IN`, not `ABORTED`. Multipart parts throw, so no multipart send succeeds.
- [sceNpWebApiCreateServicePushEventFilter](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi, PS4) - a filter for a PSN service needs the signed-in user, and no user is signed in to PSN, so a valid call returns `SIGNED_IN_USER_NOT_FOUND` after the context and handle checks, as shadPS4 does with its network disabled. `sceNpWebApiDeleteServicePushEventFilter` therefore never finds a filter.
- [sceNpWebApiInitializeForPresence, sceNpWebApiVshInitialize and sceNpWebApiIntInitialize](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - create a library context like `sceNpWebApiInitialize`, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/np/np_web_api/np_web_api.cpp#L380-L647) does on the PS4: the 32-byte internal argument structure and its `structSize` check (`INVALID_ARGUMENT`) follow its [declaration](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/np/np_web_api/np_web_api.h#L100-L106). The context type shadPS4 records and the internal name are not kept, because nothing reads them, as with `libSceNpWebApi2`. Not measured on a console
- [sceNpWebApiRegisterNotificationCallback and sceNpWebApiUnregisterNotificationCallback](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - check a null callback, then the library and user context, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/np/np_web_api/np_web_api_internal.cpp#L305-L346) does, and return 0. The callback is not kept, because no NP notification can arrive offline, so it would never be called. Not measured on a console
- [sceNpWebApiIntCreateRequest](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - creates a request like `sceNpWebApiCreateRequest`, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/np/np_web_api/np_web_api.cpp#L411-L432) does: a null group or path, or a method past PATCH, returns `INVALID_ARGUMENT` whatever the SDK version, then a content length without a type returns `INVALID_CONTENT_PARAMETER`. Its internal arguments are three pointers that shadPS4 accepts only when all are null, so a set field throws after the user context check. Not measured on a console
- [sceNpWebApiCreateContext, sceNpWebApiDeleteContext, sceNpWebApiCreateRequest, sceNpWebApiDeleteRequest, sceNpWebApiAbortRequest, sceNpWebApiSendRequest](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi, PS4) - the online id is not looked up: as in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_web_api/np_web_api_internal.cpp), a user context belongs to the initial user, so a library context holds at most one. With no user signed in to PSN, `sceNpWebApiSendRequest` returns `NOT_SIGNED_IN` unless the request is aborted. Id layouts and the delete, abort and not-found results match an offline PS4 Pro probe on firmware 10.71 (#9797); other checks follow shadPS4. Method 99 after DELETE gives `INVALID_ARGUMENT` (0x80552902) from a probe built at SDK 0x05050001; the 2.50 threshold is shadPS4's, not measured. `sceNpWebApiTerminate` also deletes user contexts and requests; shadPS4 stops after the first user context.
- [sceNpWebApiRegisterPushEventCallback, sceNpWebApiUnregisterPushEventCallback, sceNpWebApiRegisterServicePushEventCallback, sceNpWebApiUnregisterServicePushEventCallback](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi, PS4) - registered callbacks are never called, since no push event arrives. A null function is rejected from SDK 1.00 and the filter must exist in the library context from SDK 2.50, as in shadPS4; this is not checked on a console. No service filter can be created, so from SDK 2.50 a service callback returns `SERVICE_PUSH_EVENT_FILTER_NOT_FOUND`. Callback ids come from the counter of handles and filters.
