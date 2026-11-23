# libSceNKWebKit

## add Silent stubs

- [WKContext network testing and service worker controls](../../../core/libs/prx/libSceNKWebKit/src/ContextNetworkTesting.cpp) (libSceNKWebKit) - `WKContextResetServiceWorkerFetchTimeoutForTesting`, `WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting`, `WKContextSetAllowsAnySSLCertificateForWebSocketTesting`, `WKContextSetServiceWorkerFetchTimeoutForTesting` and `WKContextUseTestingNetworkSession` track certificate acceptance, service worker timeout overrides and test session flags on host `ProcessPool` instances; network process SSL verification bypass and service worker fetch timeout enforcement remain unmodeled.
