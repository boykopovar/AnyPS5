# libSceNKWebKit

## add Functional

- [Process pool network process, bandwidth, security and download client](../../../core/libs/prx/libSceNKWebKit/src/ContextSecurityAndNetwork.cpp) (libSceNKWebKit) - `WKContextGetNetworkProcessIdentifier`, `WKContextSetPrivilegedNetworkBandwidth`, `WKContextSetWebSecurityFilter`, `WKContextGetWebSecurityFilter` and `WKContextSetDownloadClient` inspect and configure network process PID, privileged network bandwidth limit, web security filter object reference and download client attachment on host `ProcessPool` instances; network daemon IPC and process filtering are unmodeled.
