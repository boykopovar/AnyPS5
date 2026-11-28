# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient::dispatchWillSendSubmitEvent](../../../core/libs/prx/libSceNKWebKit/src/EmptyClients.cpp) (libSceNKWebKit) - no-op callback per upstream WebKit 11.x [EmptyClients.cpp:608-610](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L608-L610). The exported implementation ignores the `FormState` reference; its console calling convention has not been verified. This 3.20-only symbol mangles `Ref<FormState, DumbPtrTraits<FormState>>`, which matches WebKit [EmptyClients.cpp:510-512](https://github.com/WebKit/WebKit/blob/72c72d5a8583a1cc18a0181ef16c577d862892f5/Source/WebCore/loader/EmptyClients.cpp#L510-L512) (2020), also empty; the caller owns and releases the reference.
