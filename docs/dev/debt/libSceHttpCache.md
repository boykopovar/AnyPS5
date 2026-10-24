# libSceHttpCache technical debt

### Unknown function info

- [sceHttpCacheSystemSendStatistics and sceHttpCacheSystemTerm](../../../core/libs/prx/libSceHttpCache/Export.cpp): the supplied implementations return zero without reading arguments. No callers have yet established whether their public prototypes include unused arguments.
- The following pending declarations have unverified signatures and behavior; they are parameterless import-resolution declarations and throw through `NotImplemented_nid_no_patch`: [`sceHttpCacheClearAllUserTags`](../../../core/libs/prx/libSceHttpCache/Export.cpp), [`sceHttpCacheClearCacheInfo`](../../../core/libs/prx/libSceHttpCache/Export.cpp), [`sceHttpCacheDebugSetUrl`](../../../core/libs/prx/libSceHttpCache/Export.cpp), [`sceHttpCacheRegisterCacheInfoUrl`](../../../core/libs/prx/libSceHttpCache/Export.cpp), [`sceHttpCacheRemoveTag`](../../../core/libs/prx/libSceHttpCache/Export.cpp).
