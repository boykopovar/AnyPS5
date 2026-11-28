# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient::dispatchDecidePolicyForNewWindowAction](../../../core/libs/prx/libSceNKWebKit/src/EmptyClients.cpp) (libSceNKWebKit) - no-op callback per pinned WebKit 11.x [EmptyClients.cpp:600-602](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L600-L602). The shim ignores the policy callback arguments; console ABI remains unverified. This 3.20-only symbol matches WebKit [EmptyClients.cpp:502-504](https://github.com/WebKit/WebKit/blob/72c72d5a8583a1cc18a0181ef16c577d862892f5/Source/WebCore/loader/EmptyClients.cpp#L502-L504) (2020), also empty: the `FramePolicyFunction` is neither called nor moved, and the caller destroys it.
