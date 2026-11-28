# libSceNKWebKit

## change [WKUserContentExtensionStore create, compile and lookup]

- [WKUserContentExtensionStore create, compile, lookup and remove](../../../core/libs/prx/libSceNKWebKit/src/UserContentExtensionStore.cpp) (libSceNKWebKit) - `WKUserContentExtensionStoreCreate` returns an in-memory host store instance without disk persistence for `pathRef`; user content extension store compilation, lookup and removal operate on host-managed rule lists; only the outer `[]` or `{}` delimiters are checked, so inputs such as `[garbage]` still compile into an empty rule list that applies no rules to page requests or resources, while delimiter failures report `kWKUserContentExtensionStoreCompileFailed` (`3`).
