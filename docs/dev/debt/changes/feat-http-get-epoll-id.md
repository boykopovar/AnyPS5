# libSceHttp

## add Functional

- [sceHttpGetEpollId](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - the initial epoll ID of -1, the per-object copy and the ignored `userArg` follow the reconstructed Vita 3.60 HTTP implementation at [shoui520/psp2](https://github.com/shoui520/psp2/blob/4c42e80892418c1743c98fd5f71b389f92243824/sys/internal/usermode/src/libhttp/http.c#L724-L744), not a PS5 measurement. An unwritable epoll ID output returns `SCE_HTTP_ERROR_INVALID_VALUE`, a host-safety fallback whose console result is unmeasured. `sceHttpSetEpollId` remains unimplemented.
