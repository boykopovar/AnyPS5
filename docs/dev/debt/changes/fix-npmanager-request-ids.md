# libSceNpManager

## change [sceNpCreateAsyncRequest]

- [sceNpCreateAsyncRequest](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - refuses a null parameter with `0x80550003` and a 64-bit size at `+0x00` other than `0x18` with `0x80550011`, as firmware 13.42 does ([run in an emulator](https://github.com/boykopovar/AnyPS5/issues/9277#issuecomment-6932568932), not on a console); an unreadable parameter gives `0x80550003`, a host-safety fallback. The console also stores the parameter's `+0x08` and `+0x10` in an async request and answers `sceNpPollAsync` and `sceNpWaitAsync` for it by the title's SDK version; neither is modelled, and an async request answers as a plain one here

## add Functional

- [sceNpCreateRequest, sceNpCreateAsyncRequest, sceNpPollAsync, sceNpWaitAsync, sceNpAbortRequest and sceNpDeleteRequest](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - request ids follow firmware 13.42 ([run in an emulator](https://github.com/boykopovar/AnyPS5/issues/9277#issuecomment-6932568932), not on a console): they start at `0x20000001`, the lowest free one is handed out, and a 33rd live request is refused with `0x80550013`. Poll, Wait, Abort and Delete refuse a request id of 0 or less with `0x80550003`, Poll and Wait after their null-result check. The console's library-initialized check (`0x80550002`) isn't modelled
