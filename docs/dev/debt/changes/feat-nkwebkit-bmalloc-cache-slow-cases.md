# libSceNKWebKit

## add Functional

- [bmalloc::Cache null-cache slow paths](../../../core/libs/prx/libSceNKWebKit/src/Bmalloc.cpp) (libSceNKWebKit) - not compared with a console module. The seven `Cache::*SlowCaseNullCache` exports serve allocate, aligned allocate, try-allocate, deallocate, reallocate and try-reallocate from the same host allocator as `bmalloc::api::mallocOutOfLine` and Gigacage, and ignore the `HeapKind`, which [Cache.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/bmalloc/bmalloc/Cache.cpp) only uses to pick a per-thread bmalloc heap. The crashing variants abort on failure and the `try` variants return null, as with its `FailureAction`. Closes when: compared with a console module.

## remove `_ZN7bmalloc5Cache25allocateSlowCaseNullCacheENS_8HeapKindEm`

## remove `_ZN7bmalloc5Cache25allocateSlowCaseNullCacheENS_8HeapKindEmm`

## remove `_ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv`

## remove `_ZN7bmalloc5Cache27reallocateSlowCaseNullCacheENS_8HeapKindEPvm`

## remove `_ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEm`

## remove `_ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEmm`

## remove `_ZN7bmalloc5Cache30tryReallocateSlowCaseNullCacheENS_8HeapKindEPvm`
