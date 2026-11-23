# libSceNKWebKit

## add Silent stubs

- [WKContext storage and credential maintenance](../../../core/libs/prx/libSceNKWebKit/src/ContextStorage.cpp) (libSceNKWebKit) - `WKContextClearCachedCredentials`, `WKContextClearLegacyPrivateBrowsingLocalStorage` and `WKContextSyncLocalStorage` mark cached credentials as cleared, legacy private browsing storage as cleared and local storage as synchronized on host `ProcessPool` instances; network process credential cache eviction, database file deletion and disk flush synchronization remain unmodeled.
