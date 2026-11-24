# libSceNKWebKit

## add Functional

- [Process pool plugin and auto-start configuration](../../../core/libs/prx/libSceNKWebKit/src/ContextPlugins.cpp) (libSceNKWebKit) - `WKContextSetAdditionalPluginsDirectory`, `WKContextRefreshPlugIns`, `WKContextClearSupportedPlugins`, `WKContextAddSupportedPlugin`, `WKContextSetPlugInAutoStartOriginHashes`, `WKContextSetPlugInAutoStartOrigins`, `WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime` and `WKContextCopyPlugInAutoStartOriginHashes` configure plugin search directories, supported plugin metadata registrations, and plugin auto-start origin policies on the process pool in host memory. Subprocess IPC and disk plugin loading are unmodeled.
