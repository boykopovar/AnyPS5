# libSceNKWebKit

## add Functional

- [WKContext maintenance operations](../../../core/libs/prx/libSceNKWebKit/src/ContextMaintenance.cpp) (libSceNKWebKit) - `WKContextTerminateNetworkProcess`, `WKContextSyncLocalStorage`, `WKContextClearCachedCredentials`, `WKContextClearLegacyPrivateBrowsingLocalStorage`, `WKContextUseTestingNetworkSession`, and `WKContextClearCurrentModifierStateForTesting` validate `WKContextRef` (throwing `std::invalid_argument` when null or not a `ProcessPool`), update execution state, or increment operation counters. Unverified on console.
