# libSceNKWebKit

## add Silent stubs

- [WKContextGetNetworkProcessIdentifier](../../../core/libs/prx/libSceNKWebKit/src/ContextProcessModel.cpp) (libSceNKWebKit) - host-side process-pool simulation does not spawn a separate OS network process; returns synthetic non-zero PID `1001` when `usesNetworkProcess` is enabled or `0` when disabled, unverified against a live PS5 title.
