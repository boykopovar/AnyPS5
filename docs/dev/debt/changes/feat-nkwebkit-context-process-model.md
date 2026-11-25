# libSceNKWebKit

## add Functional

- [WKContext process model, process limits, and network process operations](../../../core/libs/prx/libSceNKWebKit/src/ContextProcessModel.cpp) (libSceNKWebKit) - `WKContextSetMaximumNumberOfProcesses`, `WKContextGetMaximumNumberOfProcesses`, `WKContextSetProcessModel`, `WKContextSetUsesNetworkProcess`, `WKContextSetDiskCacheSpeculativeValidationEnabled`, `WKContextSetCanHandleHTTPSServerTrustEvaluation`, `WKContextSetCustomWebContentServiceBundleIdentifier`, and `WKContextTerminateNetworkProcess` configure process limits, process models, and network process state on `ProcessPool`. Defaults follow WebKit C API and console defaults (`maximumNumberOfProcesses` defaults to `0xffffffffu`). Console execution parity is unverified.
