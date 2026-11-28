# libSceHttpCache

## change [sceHttpCacheDebugSetUrl, sceHttpCacheSystemSendStatistics, sceHttpCacheSystemTerm and sceHttpCacheSystemShutdown]
- [sceHttpCacheDebugSetUrl, sceHttpCacheSystemSendStatistics, sceHttpCacheSystemTerm and sceHttpCacheSystemShutdown](../../../core/libs/prx/libSceHttpCache/Export.cpp): the supplied implementations return zero without reading arguments. No callers have yet established whether their public prototypes include unused arguments. On a PS5 (firmware 11.60, payload process; a title process may differ) `dlsym` on the handle of `libSceHttpCache.sprx` resolved all four names under the bare name (one run); nothing was called, so the public prototypes stay unverified.
